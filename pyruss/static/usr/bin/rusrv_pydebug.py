#! /usr/bin/env python
#
# rusrv_pydebug

# system imports
import os
import signal
import sys
import time
import traceback

#
from pyruss import Server, ConfigParser
from pyruss import ServiceTree

HELP = """\
Threaded, Python-based debug server.

/
"""

def setup_logger(config):
    global logger

    log_filename = config.get("logger", "filename")
    log_level = config.getint("logger", "level", logging.NOTSET)
    log_format = config.get("logger", "format", "[%(asctime)-15s] [%(levelname)s] %(message)s")
    logger = logging.basicConfig(filename=log_filename, format=log_format, level=log_level)
    logger = logging.getLogger()

CHARGEN_CHARS = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
class PydebugServiceTree(ServiceTree):

    def __init__(self):
        ServiceTree.__init__(self)
        self.add("/chargen", ["execute"], self.svc_chargen)
        self.add("/conn", ["execute"], self.svc_conn)
        self.add("/daytime", ["execute"], self.svc_daytime)
        self.add("/echo", ["execute"], self.svc_echo)
        self.add("/env", ["execute"], self.svc_env)
        self.add("/request", ["execute"], self.svc_request)

    def _handler(self, conn):
        print "AAA"
        req = conn.get_request()
        print "handler op (%s) spath (%s)" % (req.op, req.spath)

    def fallback_handler(self, conn):
        req = conn.get_request()
        if req.op == "help":
            os.write(conn.get_fd(1), HELP)
        else:
            ServiceTree.fallback_handler(self, conn)

    def svc_chargen(self, conn):

        fd = conn.get_fd(1)
        off = 0
        while True:
            os.write(fd, CHARGEN_CHARS[off:off+72]+"\n")
            off += 1
            if off > 94:
                off = 0
            time.sleep(0.1)
        return 0

    def svc_conn(self, conn):
        fd = conn.get_fd(1)
        try:
            creds = conn.get_creds()
            os.write(fd, "uid (%s)\ngid (%s)\npid (%d)\n" % (creds.uid, creds.gid, creds.pid))
        except:
            traceback.print_exc()
        return 0

    def svc_daytime(self, conn):
        fd = conn.get_fd(1)
        s = time.strftime("%A, %B %d, %Y %T-%Z", time.gmtime(time.time()))
        os.write(fd, s+"\n")
        return 0

    def svc_echo(self, conn):
        infd = conn.get_fd(0)
        outfd = conn.get_fd(1)
        while True:
            s = os.read(infd, 128)
            if s == "":
                break
            os.write(outfd, s)
        return 0

    def svc_env(self, conn):
        fd = conn.get_fd(1)
        for key, value in os.environ.items():
            os.write("%s=%s\n" % (key, value))
        return 0

    def svc_request(self, conn):
        fd = conn.get_fd(1)
        req = conn.get_request()
        os.write(fd, "protocol string (%s)\n" % req.protocol_string)
        os.write(fd, "spath (%s)\n" % req.spath)
        os.write(fd, "op (%s)\n" % req.op)

        attrs = conn.get_request_attrs()
        if not attrs:
            os.write(fd, "attrv (NULL)\n")
        else:
            for i, (key, value) in enumerate(attrs):
                os.write(fd, "attrv[%d] (%s=%s)\n" % (i, key, value))

        args = conn.get_request_args()
        if not args:
            os.write(fd, "argv (NULL)\n")
        else:
            for i, value in enumerate(args):
                os.write(fd, "argv[%d] (%s)\n" % (i, value))

if __name__ == "__main__":
    global config, logger

    args = sys.argv[1:]
    if len(args) < 1:
        sys.stderr.write("error: bad/missing arguments\n")
        sys.exit(-1)

    signal.signal(signal.SIGCHLD, signal.SIG_IGN)

    try:
        if 1:
            mode = 0666
            uid = os.getuid()
            gid = os.getgid()
            saddr = args[0]
        else:
            config_filename = args[0]
            config = ConfigParser()
            config.read(config_filename)

            setup_logger(config)
            logger.info("starting server")

            mode = int(config.get("server", "mode", "0666"), 8)
            uid = config.getint("server", "uid", os.getuid())
            gid = config.getint("server", "gid", os.getgid())
            saddr = config.get("server", "path")

        #s = Server(PydebugServiceTree(), "thread")
        s = Server(PydebugServiceTree(), "fork")
        s.announce(saddr, mode, uid, gid)
        s.loop()
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        sys.exit(-1)
