#! /usr/bin/env python
#
# rusrv_pythread

# system imports
import logging
import os
import signal
import sys
import threading
import time
import traceback

#
import pyruss
from pyruss import Conf
from pyruss.server import ServiceNode, Server

HELP = """\
Threading, Python-based debug server.

Provides services useful for debugging. Unless otherwise stated,
stdin, stdout, and stderr all refer to the file descriptor triple
that is returned from a russ_dial call.

/echo
    Simple echo service; receives from stdin and outputs to stdout.

/sleep <sec>
    Sleep for period of time.
"""

def setup_logger(config):
    global logger

    log_filename = config.get("logger", "filename")
    log_level = config.getint("logger", "level", logging.NOTSET)
    log_format = config.get("logger", "format", "[%(asctime)-15s] [%(levelname)s] %(message)s")
    logger = logging.basicConfig(filename=log_filename, format=log_format, level=log_level)
    logger = logging.getLogger()

class ServiceTree:

    def __init__(self):
        self.root = ServiceNode.new("", self.svc_root)
        self.root.add("echo", self.svc_echo)
        self.root.add("sleep", self.svc_sleep)

    def svc_root(self, sess):
        sconn = sess.get_sconn()
        req = sess.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_HELP:
            os.write(sconn.get_fd(1), HELP)
            sconn.exit(pyruss.RUSS_EXIT_SUCCESS)
            sconn.close()

    def svc_echo(self, sess):
        sconn = sess.get_sconn()
        req = sess.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            infd = sconn.get_fd(0)
            outfd = sconn.get_fd(1)
            while True:
                s = os.read(infd, 128)
                if s == "":
                    break
                os.write(outfd, s)
            sconn.exit(pyruss.RUSS_EXIT_SUCCESS)
            sconn.close()

    def svc_sleep(self, sess):
        sconn = sess.get_sconn()
        req = sess.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            fd = sconn.get_fd(1)
            os.write(fd, "active threads (%s)\n" % threading.active_count())
            os.write(fd, "current thread (%s)\n" % threading.current_thread)
            os.write(fd, "pid (%s)\n" % os.getpid())

            try:
                args = req.get_args()
                secs = int(args[0])
            except:
                secs = 0
            time.sleep(secs)

            sconn.exit(pyruss.RUSS_EXIT_SUCCESS)
            sconn.close()

if __name__ == "__main__":
    global config, logger

    try:
        if len(sys.argv) < 2:
            raise Exception
        config = Conf()
        config.init(sys.argv[1:])
    except:
        sys.stderr.write("error: bad/missing arguments\n")
        sys.exit(1)

    try:
        setup_logger(config)

        svr = Server.new(ServiceTree().root, pyruss.RUSS_SVR_TYPE_THREAD)
        svr.announce(config.get("server", "path"),
            int(config.get("server", "mode", "0666"), 8),
            config.getint("server", "uid", os.getuid()),
            config.getint("server", "gid", os.getgid()))
        svr.loop_thread()
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        sys.exit(1)