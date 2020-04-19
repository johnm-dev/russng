#! /usr/bin/python3
#
# rurun

# license--start
#
# Copyright 2012 John Marshall
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# license--end

from __future__ import print_function

# ----- threadpool.py -----
#
# threadpool/__init__.py
#

# license--start
#
# Copyright (c) 2016, John Marshall. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the author nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# license--end

"""ThreadPool class.
"""

class ThreadPool:
    """Manage a pool of threads used to run tasks. Tasks are added
    to a wait queue, scheduled to run when a worker is available,
    and the task return value is added to the done queue. Done tasks
    may be reaped from the done queue. The scheduling of threads to
    run may be enabled and disabled. At no time are running threads
    interrupted or terminated. A threadpool may be drained at any
    time to remove waiting tasks and halt further scheduling;
    running threads are waited on until they complete.
    """

    def __init__(self, nworkers):
        try:
            import Queue as queue
        except:
            import queue
        import threading

        self.nworkers = nworkers
        self.doneq = queue.Queue()
        self.enabled = True
        self.runs = set()
        self.waitq = queue.Queue()
        self.schedlock = threading.Lock()

    def __del__(self):
        self.enabled = False

    def _schedule(self):
        """Schedule task(s) on waiting queue if workers are
        available. Exiting tasks trigger a follow on scheduling
        operation.
        """
        import threading

        def _worker(key, fn, args, kwargs):
            try:
                rv = fn(*args, **kwargs)
            except:
                rv = None

            try:
                self.doneq.put((key, rv))
            except:
                pass
            try:
                self.runs.discard(threading.current_thread())
            except:
                pass

            self._schedule()

        try:
            self.schedlock.acquire()
            if self.enabled:
                while not self.waitq.empty() and len(self.runs) < self.nworkers:
                    args = self.waitq.get()
                    key = args[0]
                    key = key and str(key)
                    th = threading.Thread(target=_worker, name=key, args=args)
                    self.runs.add(th)
                    th.start()
        finally:
            self.schedlock.release()

    def add(self, key, fn, args=None, kwargs=None):
        """Add task to wait queue and trigger scheduler. Each tasks
        is associated with a key and optionally args and kwargs.
        """
        args = args != None and args or ()
        kwargs = kwargs != None and kwargs or {}
        self.waitq.put((key, fn, args, kwargs))
        self._schedule()

    def disable(self):
        """Disable scheduling of tasks. Does not affect running
        tasks.
        """
        self.enabled = False

    def drain(self, delay=0.5):
        """Disable scheduling, empty the wait queue, and wait until
        running tasks have completed.
        """
        import time

        self.disable()
        while not self.waitq.empty():
            self.waitq.get()
        while self.runs:
            time.sleep(delay)

    def enable(self):
        """Enable scheduling of tasks.
        """
        self.enabled = True
        self._schedule()

    def get_ndone(self):
        """Return number of completed tasks are waiting to be reaped.
        """
        return self.doneq.qsize()

    def get_nrunning(self):
        """Return number of running tasks.
        """
        return len(self.runs)

    def get_nwaiting(self):
        """Return number of waiting tasks.
        """
        return self.waitq.qsize()

    def get_nworkers(self):
        """Return number of workers.
        """
        return self.nworkers

    def has_done(self):
        """Return True is a task is done and ready to be reaped.
        """
        return self.doneq.qsize() > 0

    def has_running(self):
        """Return True if a task is currently running.
        """
        return self.runs and True or False

    def has_waiting(self):
        """Returns True if a task is waiting to be run.
        """
        return self.waitq.qsize() > 0

    def is_empty(self):
        """Returns whether there is at least 1 task in the waiting,
        running, or done state.
        """
        return not (self.has_waiting() or self.has_running() or self.has_done())

    def is_enabled(self):
        """Returns whether the scheduler is enabled.
        """
        return self.enabled

    def reap(self, block=True, timeout=None):
        """Reap result returning (key, value).
        """
        try:
            t = self.doneq.get(block, timeout)
        except:
            raise Exception("no results to reap")
        return t

    def set_nworkers(self, nworkers):
        """Adjust the number of worker threads. An increase takes
        immediate effect as new threads may be started. A decrease
        will take effect only as running threads exit.
        """
        self.nworkers = max(nworkers, 0)
        self._schedule()

# ----- threadpool.py -----

import atexit
import math
import os
from os import environ
import signal
import subprocess
import sys
import time
import traceback

sys.path.insert(1, "/usr/lib/russng")
import pyruss

DEVNULL = open("/dev/null", "w")
RURUN_EXEC_METHOD_DEFAULT = "shell"
RURUN_NRUNNING_MAX_DEFAULT = 1
RURUN_RELAY_DEFAULT = "sshr"
RURUN_TARGETSFILETYPE_DEFAULT = "legacy"
RURUN_TIMEOUT_DEFAULT = None

pnet_pid = None

def cleanup():
    if pnet_pid:
        os.kill(pnet_pid, signal.SIGHUP)

def get_target_count(pnet_addr):
    try:
        t = pyruss.dialv_wait_inouterr_timeout(10000, "execute", "%s/count" % pnet_addr)
        if t[0:2] != (0, 0):
            raise Exception()
        count = int(t[2])
        return count
    except:
        #traceback.print_exc()
        raise Exception("bad count")

def get_target_ids(targetcount, targetspec):
    """Get target ids from target spec:
    * a - individual item
    * a:b - range a to b (non-inclusive), step 1
    * a:b:c - range a to b (includes a, not b), step c (where c is
    positive or negative)
    """
    targetranges = []

    for el in targetspec.split(","):
        a, b, c = 0, 1, 1
        abc = el.split(":")
        if len(abc) > 3:
            raise Exception("bad targetspec")
        if abc[0]:
            a = int(abc[0])
        if len(abc) == 1:
            b = a+1
        else:
            b = int(abc[1] or targetcount)
            if len(abc) == 3:
                c = int(abc[2])
        targetranges.append(range(a, b, c))
    return targetranges

def gettargets(pnet_addr):
    try:
        t = pyruss.dialv_wait_inouterr_timeout(10000, "execute", "%s/gettargets" % pnet_addr)
        if t[0:2] != (0, 0):
            raise Exception()
        return t[2]
    except:
        #traceback.print_exc()
        raise Exception("cannot get targets")

def spawn_pnet_server(targetsfile, targetsfiletype):
    global pnet_pid

    try:
        pargs = ["ruspawn", "--withpids",
            "-c", "main:path=/usr/lib/russng/russpnet/russpnet_server",
            "-c", "targets:filename=%s" % targetsfile,
            "-c", "targets:filetype=%s" % targetsfiletype,
            "-c", "main:pgid=0"]
        pargs.extend(["-c", "net:relay_addr=+/sshr"])
        p = subprocess.Popen(pargs, close_fds=True,
            stdin=DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        sout, serr = p.communicate()
        if p.returncode == 0:
            t = sout.decode().split(":", 3)
            pnet_pid = int(t[0])
            return t
    except:
        traceback.print_exc()
        pass
    return None

def run_dial(taskid, ntasks, targetgid, targetid, realtargetid, targetcount, timeout, op, spath, **kwargs):
    args = kwargs["args"]
    attrs = kwargs["attrs"].copy()
    attrs["RURUN_TASKID"] = str(taskid)
    attrs["RURUN_NTASKS"] = str(ntasks)
    attrs["RURUN_TARGETGID"] = str(targetgid)
    attrs["RURUN_TARGETID"] = str(targetid)
    attrs["RURUN_REALTARGETID"] = str(realtargetid)
    attrs["RURUN_TARGETCOUNT"] = str(targetcount)

    # relay i/o as it becomes available
    connect_deadline = pyruss.to_deadline(60000)
    deadline = pyruss.to_deadline(timeout)
    infd = os.dup(0)
    if infd < 0:
        t = (-1, 0)
    else:
        cconn = pyruss.dialv(connect_deadline, op, spath, args=args, attrs=attrs)
        relay = pyruss.Relay()
        relay.add(infd, cconn.get_fd(0), closeonexit=True)
        relay.add(cconn.get_fd(1), 1)
        relay.add(cconn.get_fd(2), 2)
        rv = relay.loop(deadline, cconn.get_sysfd(0))
        t = cconn.wait(deadline)
        cconn.close()
    return t

def print_usage():
    d = {
        "PROGNAME": os.path.basename(sys.argv[0]),
        "RURUN_EXEC_METHOD_DEFAULT": RURUN_EXEC_METHOD_DEFAULT,
        "RURUN_NRUNNING_MAX_DEFAULT": RURUN_NRUNNING_MAX_DEFAULT,
        "RURUN_RELAY_DEFAULT": RURUN_RELAY_DEFAULT,
        "RURUN_TARGETSFILETYPE_DEFAULT": RURUN_TARGETSFILETYPE_DEFAULT,
        "RURUN_TIMEOUT_DEFAULT": RURUN_TIMEOUT_DEFAULT,
    }

    print("""\
usage: %(PROGNAME)s [options] <targetspec> <arg> ...
       %(PROGNAME)s [--pnet <addr>] --count

Launch a program on one or more targets. If multiple targets are
specified, the last non-0 exit value will be returned; otherwise an
exit value of 0 is returned.

Targets are specified in a file. See --targets.

If --count is specified, then the number of targets is printed.

targetspec is a comma separated list of one or more of:
* value (e.g., 10)
* start:end (e.g., 0:3 which is equivalent to 0,1,2)
* start:end:step (e.g., 0:4:2 which is equivalent to 0,2); if start
  is greater than end, step must be negative (e.g., 3:0:-1 is
  equivalent to 3,2,1)

Options:
-a|--attr <name>=<value>
    Provide attribute/environment variable settings. A
    comma-separated list of environment variable names in
    $RURUN_ENV are also passed.
-c  Run all tasks concurrently. Overrides -n.
--debug
    Enable debugging. Or set RURUN_DEBUG=1.
--exec noshell|shell|login
    Environment to launch with:
    noshell - do not use a shell (was "simple")
    shell - shell with basic environment
    login - shell with login environment
    Defaults to $RURUN_EXEC_METHOD or "%(RURUN_EXEC_METHOD_DEFAULT)s".
--gettargets
    Show targets file information.
-n <maxtasks>
    Set number of concurrently running tasks. Defaults to
    $RURUN_NRUNNING_MAX or "%(RURUN_NRUNNING_MAX_DEFAULT)s".
-N <ntasks>
    Run <ntasks> using <targetspec> settings, possibly multiple times.
    If <ntasks> is less that the number of targets in <targetspec>,
    stop at <ntasks>. Implies -c.
--pnet <addr>
    Use a given pnet address. Defaults to $RURUN_PNET_ADDR.
--relay <name>
    Use a given relay service. Defaults to $RURUN_RELAY or
    "%(RURUN_RELAY_DEFAULT)s".
--shell <path>
    Alternative shell to run on target. The arguments are passed
    to it for execution. Forces "--exec noshell".
--targetsfile <path>
    Use targets file. Defaults $RURUN_TARGETSFILE.
--targetsfiletype legacy|conf
    Targets file format type: legacy (row-oriented), conf
    (configuration file). Defaults to $RURUN_TARGETSFILETYPE or
    "%(RURUN_TARGETSFILETYPE_DEFAULT)s".
-t|--timeout <seconds>
    Allow a given amount of time to connect before aborting.
    Defaults to $RURUN_TIMEOUT or "%(RURUN_TIMEOUT_DEFAULT)s".
--wrap
    Indexes in the <targetspec> which are outside of the count are
    wrapped back to zero (effectively, modulo <count>). Negative
    indexes are wrapped, too.""" % d)

def main():
    # cleanup handler
    atexit.register(cleanup)

    rurun_allconcurrent = False
    rurun_ntasks = None
    rurun_timeout = None
    rurun_wrap = False

    rurun_debug = os.environ.get("RURUN_DEBUG") == "1"
    try:
        rurun_nrunning_max = int(os.environ.get("RURUN_NRUNNING_MAX", RURUN_NRUNNING_MAX_DEFAULT))
    except:
        rurun_nrunning_max = RURUN_NRUNNING_MAX_DEFAULT
        stderr.write('warning: falling back to RURUN_NRUNNING_MAX of "%s"' % (rurun_nrunning_max,))

    rurun_shell = None
    rurun_exec_method = os.environ.get("RURUN_EXEC_METHOD", RURUN_EXEC_METHOD_DEFAULT)
    rurun_relay = os.environ.get("RURUN_RELAY", RURUN_RELAY_DEFAULT)
    rurun_pnet_addr = os.environ.get("RURUN_PNET_ADDR")
    rurun_targetsfile = os.environ.get("RURUN_TARGETSFILE")
    rurun_targetsfiletype = os.environ.get("RURUN_TARGETSFILETYPE", RURUN_TARGETSFILETYPE_DEFAULT)

    try:
        rurun_timeout = os.environ.get("RURUN_TIMEOUT", RURUN_TIMEOUT_DEFAULT)
        if rurun_timeout != None:
            rurun_timeout = int(rurun_timeout)
    except:
        rurun_timeout = RURUN_TIMEOUT_DEFAULT
        stderr.write('warning: falling back to RURUN_TIMEOUT of "%s"' % (rurun_timeout,))

    args = sys.argv[1:]
    if len(args) == 1 and args[0] in ["-h", "--help"]:
        print_usage()
        sys.exit(0)
    elif len(args) < 2 and (args and args[0] not in ["--count", "--gettargets"]):
        #traceback.print_exc()
        sys.stderr.write("error: bad/missing arguments\n")
        sys.exit(1)

    # command-line handling
    try:
        attrs = {}
        mpi = 0
        nrunningmax = rurun_nrunning_max
        rurun_gettargets = False
        showcount = False
        targetspec = None

        while args:
            arg = args.pop(0)
            if arg in ["-a", "--attr"] and args:
                attrs[arg] = args.pop(0)
            elif arg == "-c":
                rurun_allconcurrent = True
            elif arg == "--count":
                showcount = True
            elif arg == "--debug":
                rurun_debug = "1"
            elif arg == "--exec" and args:
                rurun_exec_method = args.pop(0)
            elif arg == "--gettargets":
                rurun_gettargets = True
            elif arg == "-n" and args:
                nrunningmax = int(args.pop(0))
            elif arg == "-N" and args:
                rurun_ntasks = int(args.pop(0))
                rurun_allconcurrent = True
                rurun_wrap = True
            elif arg == "--pnet" and args:
                rurun_pnet_addr = args.pop(0)
            elif arg == "--relay" and args:
                rurun_relay = args.pop(0)
            elif arg == "--shell" and args:
                rurun_shell = args.pop(0)
            elif arg in ["-t", "--timeout"] and args:
                rurun_timeout = int(args.pop(0))
            elif arg == "--targetsfile" and args:
                rurun_targetsfile = args.pop(0)
            elif arg == "--targetsfiletype" and args:
                rurun_targetsfiletype = args.pop(0)
            elif arg == "--wrap":
                rurun_wrap = True
            else:
                targetspec = arg
                break

        if None in [targetspec] and not (showcount or rurun_gettargets):
            raise Exception()
    except:
        #traceback.print_exc()
        sys.stderr.write("error: bad/missing argument\n")
        sys.exit(1)

    try:
        # expand
        rurun_targetsfile = os.path.realpath(rurun_targetsfile)

        # dynamically start pnet server if necessary
        if not rurun_pnet_addr:
            if rurun_targetsfile:
                t = spawn_pnet_server(rurun_targetsfile, rurun_targetsfiletype)
                pnetpid, pnetpgid, rurun_pnet_addr = t
                #print(rurun_pnet_addr, pnetpid, pnetpgid)
                pnetpid = int(pnetpid)
                pnetpgid = int(pnetpgid)
        if not rurun_pnet_addr:
            raise Exception()
    except:
        #traceback.print_exc()
        sys.stderr.write("error: neither RURUN_PNET_ADDR nor RURUN_TARGETSFILE are set\n")
        sys.exit(1)

    try:
        if rurun_gettargets:
            print(gettargets(rurun_pnet_addr).decode(), end='')
            sys.exit(0)

        targetcount = get_target_count(rurun_pnet_addr)
        if showcount:
            print(targetcount)
            sys.exit(0)

        # convert targetspec to list of targets
        targetranges = get_target_ids(targetcount, targetspec)

        if rurun_debug:
            # debugging output
            l = [
                "-----",
                "targetspec (%s)" % targetspec,
                "targets (%s)" % str(targetranges),
                "targets file (%s)" % rurun_targetsfile,
                "targets file type (%s)" % rurun_targetsfiletype,
                "pnet addr (%s)" % rurun_pnet_addr,
                "exec method (%s)" % rurun_exec_method,
                "relay (%s)" % rurun_relay,
                "shell (%s)" % rurun_shell,
                "timeout (%s)" % rurun_timeout,
                "RURUN_ENV (%s)" % environ.get("RURUN_ENV"),
                "RUMPIRUN_ENV (%s)" % environ.get("RUMPIRUN_ENV"),
                "-----",
            ]
            sys.stderr.write("%s\n" % "\n".join(l))


        # collect attrs
        names = environ.get("RURUN_ENV", "").replace(",", " ").split()
        for name in names:
            if name not in attrs:
                attrs[name] = environ.get(name, "")

        # select exec method and "shell"
        if rurun_shell:
            # override
            rurun_exec_method = "noshell"

        # mpirun-specific
        # TODO: eliminate?
        if environ.get("HYDRA_LAUNCHER") == "ssh":
            # drop "-x"
            args.pop(0)

        # TODO: ensure self and all children are killed/cleaned up
        #trap '' HUP
        #trap 'exit 1' INT TERM

        targetpairs = [(targetgid, targetid) for targetgid, targetids in enumerate(targetranges) for targetid in targetids]
        speccount = len(targetpairs)
        if rurun_ntasks == None:
            rurun_ntasks = speccount

        if rurun_allconcurrent:
            nrunningmax = rurun_ntasks

        # update targetpairs to match rurun_ntasks
        m = int(math.ceil(float(rurun_ntasks)/speccount))
        targetpairs = (targetpairs*m)[:rurun_ntasks]

        tp = ThreadPool(nrunningmax)

        procs = []
        taskid = 0
        timeout = rurun_timeout == None and pyruss.to_timeout(pyruss.RUSS_DEADLINE_NEVER) or rurun_timeout
        for taskid, (targetgid, targetid) in enumerate(targetpairs):
            #print("targetid (%s) targetids (%s)" % (targetid, targetids))
            exec_method = rurun_exec_method == "noshell" and "simple" or rurun_exec_method
            spath = os.path.join(rurun_pnet_addr, "run", (rurun_wrap and ":" or "")+str(targetid), rurun_exec_method)
            if rurun_exec_method == "noshell":
                sargs = args[:]
            else:
                # must combine args to pass as command string to shell
                sargs = []
                for arg in args:
                    if arg == "":
                        arg = '""'
                    sargs.append(arg)
                sargs = [" ".join(sargs)]

            if rurun_shell:
                sargs.insert(0, rurun_shell)

            if rurun_debug:
                pass

            targs = [taskid, rurun_ntasks, targetgid, targetid, targetid % targetcount, targetcount, timeout, "execute", spath]
            tattrs = {"args": sargs, "attrs": attrs}
            tp.add(taskid, run_dial, targs, tattrs)

        finalev = 0
        for _ in range(rurun_ntasks):
            targetid, t = tp.reap()
            rv, ev = t
            if rv == 0:
                if ev != 0:
                    finalev = ev
            else:
                finalev = 1

        sys.exit(finalev)
    except SystemExit:
        raise
    except:
        #traceback.print_exc()
        sys.stderr.write("error: unexpected situation\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
