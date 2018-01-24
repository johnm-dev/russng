#! /usr/bin/env python
#
# pyruss/server.py

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

"""New server and support implementation to the russ library C API.
It is intentionally thin!

Rules:
* most objects simply wrap C "objects"; this means that C "objects"
must be explicitly destroyed rather than automatically on the python
side (e.g., via __del__)
* be careful that C "objects" are not referenced multiple times via
multiple python objects
"""

# system imports
import sys
import threading

#
import pyruss
from pyruss import libruss, SVCHANDLER_FUNC
from pyruss import Request, ServerConn

def init(conf):
    """Create a Server
    """
    if conf == None:
        return None

    sd = conf.getint("main", "sd", pyruss.RUSS_SVR_LIS_SD_DEFAULT)
    accepttimeout = conf.getint("main", "accepttimeout", pyruss.RUSS_SVR_TIMEOUT_ACCEPT)
    closeonaccept = conf.getint("main", "closeonaccept", 0)
    root = ServiceNode.new("", None)
    if root == None:
        return None
    svr = Server.new(root, 0, sd)
    if svr == None:
        return None
    if svr.set_accepttimeout(accepttimeout) < 0:
        return None
    if svr.set_closeonaccept(closeonaccept) < 0:
        return None
    return svr

class ServiceHandler:
    def __init__(self, handler):
        self.handler = handler

    def _handler(self, sess_ptr):
        self.handler(Sess(sess_ptr))

service_handlers = {}
def get_service_handler(handler):
    if handler not in service_handlers:
        service_handlers[handler] = SVCHANDLER_FUNC(ServiceHandler(handler)._handler)
    return service_handlers[handler]

class ServiceNode:
    """Wrapper for russ_svcnode object and associated methods.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    @classmethod
    def new(cls, name, handler):
        _ptr = libruss.russ_svcnode_new(name, get_service_handler(handler))
        if _ptr == None:
            raise Exception("could not create ServiceNode")
        return cls(_ptr)

    def free(self):
        """Free underlying object.
        """
        libruss.russ_svcnode_free(self._ptr)
        self._ptr = None

    def add(self, name, handler):
        """Add handler.
        """
        return ServiceNode(libruss.russ_svcnode_add(self._ptr, name, get_service_handler(handler)))

    def find(self, path):
        """Find service node corresponding to service path.
        """
        return ServiceNode(libruss.russ_svcnode_find(self._ptr, path))

    def set_autoanswer(self, value):
        """Set autoanswer state.
        """
        return libruss.russ_svcnode_set_autoanswer(self._ptr, value)

    def set_virtual(self, value):
        """Set virtual state.
        """
        return libruss.russ_svcnode_set_virtual(self._ptr, value)

    def set_wildcard(self, value):
        """Set wildcard state.
        """
        return libruss.russ_svcnode_set_wildcard(self._ptr, value)

class Server:
    """Wrapper for russ_svr object and associated methods.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    @classmethod
    def new(cls, root, typ, lisd):
        _ptr = libruss.russ_svr_new(root._ptr, typ, lisd)
        if not bool(_ptr):
            raise Exception("could not create Server")
        return cls(_ptr)

    def accept(self, deadline):
        """Accept connection and return ServerConn object.
        """
        sconn_ptr = libruss.russ_svr_accept(self._ptr, deadline)
        return bool(sconn_ptr) and ServerConn(sconn_ptr) or None

    def free(self):
        """Free underlying object.
        """
        libruss.russ_svr_free(self._ptr)
        self._ptr = None

    def get_closeonaccept(self):
        return self._ptr.contents.closeonaccept

    def get_lisd(self):
        return self._ptr.contents.lisd

    def handler(self, sconn):
        """Default handler.
        """
        libruss.russ_svr_handler(self._ptr, sconn._ptr)

    def loop(self):
        """Dispath to appropriate server loop.
        """
        if self._ptr.contents.type == pyruss.RUSS_SVR_TYPE_THREAD:
            self.loop_thread()
        else:
            libruss.russ_svr_loop(self._ptr)

    def loop_thread(self):
        """Threaded loop.

        Requires helper.
        """
        def helper(svr, sconn):
            try:
                self.handler(sconn)
                # failsafe exit info (if not provided)
                sconn.fatal(pyruss.RUSS_MSG_NOEXIT, pyruss.RUSS_EXIT_SYSFAILURE)
                sconn.free()
            except:
                pass

        while self.get_lisd() >= 0:
            sconn = self.accept(pyruss.to_deadline(self._ptr.contents.accepttimeout))
            if self.get_closeonaccept():
                os.close(self.get_lisd())
                self.set_lisd(-1)
            if not sconn:
                sys.stderr.write("error: cannot accept connection\n")
                continue
            th = threading.Thread(target=helper, args=(self, sconn))
            if th:
                th.start()
            else:
                sys.stderr.write("error: cannot spawn thread\n")

    def set_accepttimeout(self, value):
        """Set acceptimeout value.
        """
        return libruss.russ_svr_set_accepttimeout(self._ptr, value)

    def set_autoswitchuser(self, value):
        """Set autoswitchuser state.
        """
        return libruss.russ_svr_set_autoswitchuser(self._ptr, value)

    def set_closeonaccept(self, value):
        """Set closeonaccept flag.
        """
        return libruss.russ_svr_set_closeonaccept(self._ptr, value)

    def set_help(self, value):
        """Set help text.
        """
        return libruss.russ_svr_set_help(self._ptr, value)

    def set_lisd(self, lisd):
        """Set socket descriptor.
        """
        return libruss.russ_svr_set_lisd(self._ptr, lisd)

    def set_root(self, root):
        """Set root ServiceNode.
        """
        return libruss.russ_svr_set_root(self._ptr, root._ptr)

    def set_type(self, stype):
        """Set server type.
        """
        return libruss.russ_svr_set_type(self._ptr, stype)

class Sess:
    """Wrapper for russ_sess.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    def __del__(self):
        self._ptr = None

    def get_request(self):
        """Return Request object.
        """
        return self._ptr.contents.req and Request(self._ptr.contents.req) or None

    def get_sconn(self):
        """Return ServerConn object.
        """
        return self._ptr.contents.sconn and ServerConn(self._ptr.contents.sconn) or None

    def get_spath(self):
        """Return service path.
        """
        return self._ptr.contents.spath

    def get_svr(self):
        """Return Server object.
        """
        return self._ptr.contents.svr and Server(self._ptr.contents.svr) or None
