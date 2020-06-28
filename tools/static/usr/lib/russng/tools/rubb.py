#! /usr/bin/python3
#
# rubb.py

import os
import os.path
import shutil
import signal
import stat
import subprocess
import sys
from sys import stderr
import traceback

# system
ETC_DIR = "/etc/russ"
RUN_DIR = "/var/run/russ"
CONF_DIR = "%s/conf" % RUN_DIR
PIDS_DIR = "%s/pids" % RUN_DIR
SERVICES_DIR = "%s/services" % RUN_DIR
SYSTEM_SOURCESFILE = "%s/bb.sources" % ETC_DIR
SYSTEM_BBBASEDIR = "%s/bb" % RUN_DIR
SYSTEM_SAFEPATHS = ["/run/russ/bb", "/var/run/russ/bb"]

# user
USER_BBBASEDIR = os.path.expanduser("~/.russ/bb")
USER_SAFEPATHS = [USER_BBBASEDIR]

DEVNULL = open("/dev/null", "w")

class BB:
    """Manage bulletin board (BB) for RUSS services.

    Organized as:
        .../bb/
          <bbname>/
            conf/
            pids/
            services/

    The pids dir is only used for "system" BBs.
    """

    def __init__(self, bbdir):
        self.bbdir = bbdir

        self.name = os.path.basename(bbdir)
        self.confdir = os.path.join(self.bbdir, "conf")
        self.pidsdir = os.path.join(self.bbdir, "pids")
        self.servicesdir = os.path.join(self.bbdir, "services")

    def prep(self):
        """Ensure working areas exist.
        """
        print("prepping bb (%s) ..." % (self.name,))
        for dirpath in [self.confdir, self.pidsdir, self.servicesdir]:
            if not os.path.isdir(dirpath):
                print("makedir (%s)" % (dirpath,))
                os.makedirs(dirpath)

    def clean(self):
        """Clean areas associated with srcname.
        """
        print("cleaning bb (%s) ..." % (self.name,))
        for dirpath in [self.confdir, self.pidsdir, self.servicesdir]:
            if os.path.exists(dirpath):
                for safepath in safepaths:
                    if dirpath.startswith(safepath):
                        for name in os.listdir(dirpath):
                            path = os.path.join(dirpath, name)
                            os.remove(path)
                if not os.listdir(dirpath):
                    os.rmdir(dirpath)
        if os.path.exists(self.bbdir):
            if not os.listdir(self.bbdir):
                os.rmdir(self.bbdir)

    def get_confnames(self):
        """Return configuration names without the .conf.
        """
        _, _, names = next(os.walk(self.confdir))
        names = [name[:-5] for name in names]
        return names

    def get_names(self):
        """Return all names found under conf/ and services/.
        """
        if os.path.isdir(self.confdir):
            _, _, confnames = next(os.walk(self.confdir))
        else:
            confnames = []
        if os.path.isdir(self.servicesdir):
            _, _, servicenames = next(os.walk(self.servicesdir))
        else:
            servicenames = []
        names = [name[:-5] for name in confnames if name.endswith(".conf")]
        names.extend(servicenames)
        return set(names)

    def get_servernames(self):
        """List server names.
        """
        names = os.listdir(self.servicesdir)
        return names

    def get_server(self, name):
        return BBServer(self, name)

    def install(self, filename, newname=None):
        """Install file contents to configuration file.
        """
        self.prep()

        if newname:
            name = newname
        else:
            name = os.path.basename(filename)
            if name.endswith(".conf"):
                name = name[:-5]

        print("installing (%s) from file (%s)" % (name, filename))
        txt = open(filename).read()
        bs = self.get_server(name)
        bs.install(txt)

    def remove(self, name):
        """Remove configuration.
        """
        bs = self.get_server(name)
        if bs:
            bs.removeconf()

    def show(self, name):
        """Show configuration.
        """
        bs = self.get_server(name)
        if bs:
            txt = bs.get_conf()
            if txt:
                print(txt)

    def start_servers(self, names):
        """Start select named or all servers of a BB.
        """
        print("starting servers for bb (%s) ..." % (self.name,))
        for name in names:
            bs = self.get_server(name)
            if bs.isrunning():
                stderr.write("warning: server (%s) already running\n" % (name,))
            else:
                bs.start()
            st = bs.get_status()
            if st:
                print("bb=%(bbname)s:name=%(name)s:running=%(isrunning)s" % st)

    def status_servers(self, names, detail=False):
        """Output status of select named or all servers of a BB.
        """
        for name in names:
            bs = self.get_server(name)
            st = bs.get_status()
            if st:
                if detail:
                    print("bb=%(bbname)s:name=%(name)s:running=%(isrunning)s:type=%(type)s:pid=%(pid)s:conffile=%(conffile)s:servicefile=%(servicefile)s" % st)
                else:
                    print("bb=%(bbname)s:name=%(name)s:running=%(isrunning)s" % st)

    def stop_servers(self, names):
        """Stop select named or all servers of a BB.
        """
        print("stopping servers for bb (%s) ..." % (self.name,))
        for name in names:
            bs = self.get_server(name)
            if bs.isrunning():
                bs.stop()
            st = bs.get_status()
            if st:
                print("bb=%(bbname)s:name=%(name)s:running=%(isrunning)s" % st)

    def sync(self, sources, tags=None, preclean=False):
        """Sync configuration from sources to BB.

        Configurations that are not found in the sources are cleaned.
        """
        print("syncing bb (%s) ..." % (self.name,))
        if tags:
            sources = [d for d in sources if d["name"] in tags]

        self.prep()
        foundfilenames = set([name for name in os.listdir(self.confdir) if name.endswith(".conf")])

        if preclean:
            for filename in foundfilenames:
                name = filename[:-5]
                s = self.get_server(name)
                s.stop()
                s.clean()

        syncfilenames = []
        for d in sources:
            srctype = d["type"]
            srcpath = d["source"]
            if srctype == "dir":
                filenames = [name for name in os.listdir(srcpath) if name.endswith(".conf")]
                for filename in filenames:
                    name = filename[:-5]
                    if filename in syncfilenames:
                        stderr.write("error: cannot sync duplicate name (%s) from source (%s)\n" % (name, d["name"]))
                        continue

                    txt = open(os.path.join(srcpath, filename)).read()
                    s = BBServer(self, name)
                    print("installing (%s) from source (%s)" % (name, d["name"]))
                    s.install(txt)
                    syncfilenames.append(filename)

        # clean
        if not tags:
            for filename in foundfilenames.difference(syncfilenames):
                name = filename[:-5]
                s = BBServer(self, name)
                print("cleaning (%s)" % (name,))
                s.clean()

class BBServer:
    """Manage server under BB location.
    """

    def __init__(self, bb, name):
        self.bb = bb
        self.name = name

        self.confname = "%s.conf" % (name,)
        self.conffile = os.path.join(self.bb.confdir, self.confname)
        self.pidfile = os.path.join(self.bb.pidsdir, self.name)
        self.servicefile = os.path.join(self.bb.servicesdir, self.name)

    def _getpid(self):
        try:
            return int(open(self.pidfile).read())
        except:
            return None

    def _hasserviceconffile(self):
        try:
            line = open(self.conffile).readline()
            return " service=conffile" in line
        except:
            return False

    def _killpid(self):
        if self.isrunning():
            pid = self._getpid()
            os.kill(-pid, signal.SIGHUP)
        self._removepid()

    def _removepid(self):
        try:
            os.remove(self.pidfile)
        except:
            pass

    def _removeservice(self):
        if os.path.exists(self.servicefile):
            os.remove(self.servicefile)

    def _ruspawn(self):
        pargs = [
            "ruspawn",
            "-f", self.conffile,
            "-c", "main:pgid=0",
            "-c", "main:addr=%s" % (self.servicefile,)
        ]
        p = subprocess.Popen(pargs,
            stdin=DEVNULL,
            #stdout=DEVNULL,
            #stderr=DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            close_fds=True)
        out, err = p.communicate()
        if debug:
            print("pargs (%s)" % (pargs,))
            print("pid (%s) out (%s) err (%s)" % (p.pid, out, err))
        if p.pid == None:
            return False
        self._setpid(p.pid)
        return True

    def _setpid(self, pid):
        open(self.pidfile, "w+").write("%s" % (pid,))

    def clean(self):
        """Clean server items
        """
        self.removeconf()
        self._removepid()
        self._removeservice()

    def get_conf(self):
        try:
            return open(self.conffile).read()
        except:
            pass

    def get_status(self):
        """Return status information.
        """
        d = {
            "bbname": bb.name,
            "conffile": os.path.exists(self.conffile) and self.conffile or None,
            "isrunning": self.isrunning(),
            "name": self.name,
            "pid": self._getpid(),
            "pidfile": os.path.exists(self.pidfile) and self.pidfile or None,
            "servicefile": os.path.exists(self.servicefile) and self.servicefile or None,
            "type": self.isconffile() and "conffile" or "socket",
        }
        return d

    def install(self, txt):
        """Install configuration file.
        """
        open(self.conffile, "w+").write(txt)

    def isconffile(self):
        """Check if servicefile is conffile rather than a socket file.
        """
        try:
            st = os.stat(self.servicefile)
            if stat.S_ISSOCK(st.st_mode):
                return False
            return self._hasserviceconffile()
        except:
            if debug:
                traceback.print_exc()
            return False

    def isrunning(self):
        """Check if server is running.

        A running server has a servicefile and a pidfile.
        """
        try:
            if os.path.exists(self.pidfile):
                pid = open(self.pidfile).read()
                os.kill(-int(pid), 0)
                return True
            else:
                return self.isconffile()
        except:
            if debug:
                traceback.print_exc()
            return False

    def removeconf(self):
        if os.path.exists(self.conffile):
            os.remove(self.conffile)

    def restart(self):
        self.stop()
        self.start()

    def start(self):
        if self._hasserviceconffile():
            shutil.copy(self.conffile, self.servicefile)
        else:
            self._ruspawn()

    def stop(self):
        if not self.isconffile():
            self._killpid()
        self._removeservice()

class SourcesFile:
    """Interface to working with the bb.sources file.
    """

    def __init__(self, path=None):
        self.path = path
        self.d = None

    def get_sources(self, bbname):
        """Get sources associated with name from sources file.
        """
        self.load()
        return self.d.get(bbname)

    def get_names(self):
        """Get names from sources file.
        """
        self.load()
        return list(self.d.keys())

    def load(self, force=False):
        """Load sources file.

        Use force to reload.
        """
        if not force and self.d != None:
            return

        d = {}
        for line in open(self.path).readlines():
            line = line.strip()
            if line == "" or line.startswith("#"):
                continue
            t = line.split(":")
            bbname = t[0]
            l = d.setdefault(bbname, [])
            d2 ={
                "name": t[1],
                "type": t[2],
                "source": t[3],
            }
            l.append(d2)
        self.d = d

def get_bbdir(name=None):
    """Return bbdir based on user and optional bb name.

    If name starts with "/", then return it as the bbdir. Otherwise,
    name cannot contain a "/".
    """
    if name and name.startswith("/"):
        return name
    if name and "/" in name:
        return None
    bbdir = os.path.join(bbbasedir, name)
    return bbdir

def get_bbnames(bbbasedir):
    """Return list of BB names.
    """
    _, bbnames, _ = next(os.walk(bbbasedir))
    return bbnames

def print_usage():
    d = {
        "progname": os.path.basename(sys.argv[0]),
        "bbbasedir": bbbasedir,
        "confdir": confdir,
        "sourcesfile": sourcesfile,
    }
    print("""\
usage: %(progname)s [<options>] <cmd> [<cmdoptions>]
       %(progname)s -h|--help|help

Manage system or user RUSS bulletin boards (BB). A BB hosts RUSS
services. Although the services can be accessed directly using a
path, the standard way is to use the ("+") plus service. By default,
the plus server searches for services at some system ("system") and
user ("override", "fallback") BBs.

System BBs can host services by either a socket (running) or
configuration file (run on demand). The user BBs host services by
configuration only.

For system BBs, a bb.sources file is used to specify the
configuration sources used to set up. Use the "sync" command to
syncronize from the sources to the BB. The default sources file is
at:
    %(sourcesfile)s

System BBs are located at:
    %(bbbasedir)s

For user BBs, configurations are installed using the the "install"
command.

Options:
--bb <bbname>   Operate on the named BB. System default is "system",
                User default is "override".
--sources <path>
                (system) Alternate path of the bb.sources file.

Operations:
clean           Clean BB.
install <filename> [<newname>]
                Install configuration (filename ends with .conf). Use
                <newname> to override name derived from <filename>.
list [-l]       List BB entries. Use -l for details.
list-bb         List BBs.
list-sources    (system) List sources from sources file.
remove <name>   Remove configuration.
restart [<name>,...]
                Restart server(s).
resync          (system) Call clean+sync.
show <name>     Show configuration.
start [<name>,...]
                Start server(s). Make available for use.
status [-l] [<name>,...]
                Report status of server(s). Use -l for details.
stop [<name>,...]
                Stop server(s). Make unavailable for use.
sync [<tag>,...]
                (system) Syncronize local configuration used sources
                specified in a bb.sources file. Use <tag> to limit
                sources to use.""" % d)

if __name__ == "__main__":
    if not os.path.exists(SYSTEM_SOURCESFILE):
        stderr.write("error: missing bb.sources file\n")
        sys.exit(1)

    try:
        args = sys.argv[1:]

        bbbasedir = None
        bbdir = None
        bbname = None
        cmd = None
        confdir = None
        debug = os.environ.get("RUBB_DEBUG") == "1"
        sf = None
        sourcesfile = None
        usertype = None
        verbose = os.environ.get("RUBB_VERBOSE") == "1"

        if os.getuid() == 0:
            bbbasedir = SYSTEM_BBBASEDIR
            bbname = "system"
            safepaths = SYSTEM_SAFEPATHS
            sourcesfile = SYSTEM_SOURCESFILE
            sf = SourcesFile(sourcesfile) # after sourcesfile!
            usertype = "system"
        else:
            bbbasedir = USER_BBBASEDIR
            bbname = "override"
            safepaths = USER_SAFEPATHS
            usertype = "user"

        bbdir = get_bbdir(bbname)
        bb = BB(bbdir)

        while args:
            arg = args.pop(0)

            if arg == "--bb" and args:
                bbname = args.pop(0)
            elif arg == "--bbbasedir" and args:
                bbbasedir = args.pop(0)
            elif arg == "--debug":
                debug = True
            elif arg in ["-h", "--help", "help"]:
                print_usage()
                sys.exit(0)
            elif arg == "--sources" and args:
                sourcespath = args.pop(0)
                sf = SourcesFile(sourcesfile)
            elif arg == "--verbose":
                verbose = True
            else:
                cmd = arg
                break

        bbdir = get_bbdir(bbname)
        if not bbdir:
            stderr.write("error: bad bb name (%s)\n" % (bbname,))
            sys.exit(1)
        bb = BB(bbdir)

        if not cmd:
            raise Exception()
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        stderr.write("error: bad/missing arguments\n")
        sys.exit(1)

    try:
        if verbose:
            print("bb basedir (%s)" % (bbbasedir,))
            print("bb name (%s)" % (bbname,))
            print("bb dir (%s)" % (bbdir,))
            print("sources file (%s)" % (sourcesfile,))
            print("cmd (%s)" % (cmd,))

        if cmd == "clean" and not args:
            names = sorted(bb.get_names())
            bb.stop_servers(names)
            bb.clean()
        elif cmd == "install" and args:
            filename = None
            newname = None

            filename = args.pop(0)
            if args:
                newname = args.pop(0)
            bb.install(filename, newname)
        elif cmd == "list" and not args:
            names = sorted(bb.get_names())
            if names:
                print("%s" % " ".join(names))
        elif cmd == "list-bb":
            bbnames = get_bbnames(bbbasedir)
            if bbnames:
                print(" ".join(bbnames))
        elif cmd == "list-sources" and not args:
            sources = sf.get_sources(bb.name)
            if sources:
                print("%s" % " ".join([d["name"] for d in sources]))
        elif cmd == "remove" and len(args) == 1:
            name = args.pop(0)
            bb.remove(name)
        elif cmd == "restart" and len(args) < 2:
            names = args and [args.pop(0)] or sorted(bb.get_names())
            bb.stop_servers(names)
            bb.start_servers(names)
        elif cmd == "resync":
            names = sorted(bb.get_names())
            bb.stop_servers(names)
            bb.clean()
            bb.sync(sf.get_sources(bb.name))
        elif cmd == "show" and len(args) == 1:
            name = args.pop(0)
            bb.show(name)
        elif cmd == "start" and len(args) < 2:
            names = args and [args.pop(0)] or sorted(bb.get_names())
            bb.start_servers(names)
        elif cmd == "status" and len(args) < 2:
            detail = False

            while args:
                arg = args.pop(0)
                if arg == "-l":
                    detail = True
                else:
                    args.insert(0, arg)
                    break

            names = args and [args.pop(0)] or sorted(bb.get_names())
            bb.status_servers(names, detail)
        elif cmd == "stop" and len(args) < 2:
            names = args and [args.pop(0)] or sorted(bb.get_names())
            bb.stop_servers(names)
        elif cmd == "sync" and len(args) <= 1:
            tags = None
            if args:
                tags = args.pop(0).split(",")
            bb.sync(sf.get_sources(bb.name), tags)
        else:
            stderr.write("error: bad/missing command or arguments\n")
            sys.exit(1)
    except SystemExit:
        raise
    except:
        if debug:
            traceback.print_exc()
        stderr.write("error: fail to run command\n")
        sys.exit(1)

    sys.exit(0)
