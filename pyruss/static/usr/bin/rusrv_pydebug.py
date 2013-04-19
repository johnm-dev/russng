#! /usr/bin/env python
#
# rusrv_pydebug

# system imports
import logging
import os
import signal
import sys
import time
import traceback

#
import pyruss
from pyruss import Conf
from pyruss.server import ServiceNode, Server

HELP = """\
Forking, Python-based debug server.

Provides services useful for debugging. Unless otherwise stated,
stdin, stdout, and stderr all refer to the file descriptor triple
that is returned from a russ_dial call.

/chargen
    Character generator outputting to stdout; follows the RPC 864
    the RFC 864 protocol sequence.

/conn
    Outputs russ connection information.

/daytime
    Outputs the date and time to stdout.

/echo
    Simple echo service; receives from stdin and outputs to stdout.

/env
    Outputs environ entries to stdout.

/request
    Outputs the request information to the server stdout.
"""

def setup_logger(config):
    global logger

    log_filename = config.get("logger", "filename")
    log_level = config.getint("logger", "level", logging.NOTSET)
    log_format = config.get("logger", "format", "[%(asctime)-15s] [%(levelname)s] %(message)s")
    logger = logging.basicConfig(filename=log_filename, format=log_format, level=log_level)
    logger = logging.getLogger()

CHARGEN_CHARS = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
class ServiceTree:

    def __init__(self):
        self.root = ServiceNode.new("/", self.svc_root)
        self.root.add("chargen", self.svc_chargen)
        self.root.add("conn", self.svc_conn)
        self.root.add("daytime", self.svc_daytime)
        self.root.add("echo", self.svc_echo)
        self.root.add("env", self.svc_env)
        self.root.add("request", self.svc_request)

    def svc_root(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_HELP:
            os.write(conn.get_fd(1), HELP)
            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

    def svc_chargen(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            fd = conn.get_fd(1)
            off = 0
            while True:
                os.write(fd, CHARGEN_CHARS[off:off+72]+"\n")
                off += 1
                if off > 94:
                    off = 0
                time.sleep(0.1)
            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

    def svc_conn(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            fd = conn.get_fd(1)
            try:
                creds = conn.get_creds()
                os.write(fd, "uid (%s)\ngid (%s)\npid (%d)\n" % (creds.uid, creds.gid, creds.pid))
                conn.exit(pyruss.RUSS_EXIT_SUCCESS)
                conn.close()
            except:
                traceback.print_exc()

    def svc_daytime(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            fd = conn.get_fd(1)
            s = time.strftime("%A, %B %d, %Y %T-%Z", time.gmtime(time.time()))
            os.write(fd, s+"\n")
            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

    def svc_echo(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            infd = conn.get_fd(0)
            outfd = conn.get_fd(1)
            while True:
                s = os.read(infd, 128)
                if s == "":
                    break
                os.write(outfd, s)
            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

    def svc_env(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
            fd = conn.get_fd(1)
            for key, value in os.environ.items():
                os.write(fd, "%s=%s\n" % (key, value))
            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

    def svc_request(self, sess):
        conn = sess.get_conn()
        req = conn.get_request()
        if req.opnum == pyruss.RUSS_OPNUM_EXECUTE:
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

            conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            conn.close()

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

        svr = Server.new(ServiceTree().root, pyruss.RUSS_SVR_TYPE_FORK)
        svr.announce(config.get("server", "path"),
            int(config.get("server", "mode", "0666"), 8),
            config.getint("server", "uid", os.getuid()),
            config.getint("server", "gid", os.getgid()))
        svr.loop()
        #s = Server(PydebugServiceTree(), "fork")
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        sys.exit(-1)
