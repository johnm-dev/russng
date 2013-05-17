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
* 
* be careful that C "objects" are not referenced multiple times via
multiple python objects
"""

# system imports
import sys
import threading

#
import pyruss
from pyruss import libruss, SVC_HANDLER_FUNC
from pyruss import Listener, Request, ServerConn

class ServiceHandler:
    def __init__(self, handler):
        self.handler = handler

    def _handler(self, sess_ptr):
        self.handler(Sess(sess_ptr))

service_handlers = {}
def get_service_handler(handler):
    if handler not in service_handlers:
        service_handlers[handler] = SVC_HANDLER_FUNC(ServiceHandler(handler)._handler)
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
        libruss.russ_svcnode_free(self._ptr)
        self._ptr = None

    def add(self, name, handler):
        return ServiceNode(libruss.russ_svcnode_add(self._ptr, name, get_service_handler(handler)))

    def find(self, path):
        return ServiceNode(libruss.russ_svcnode_find(self._ptr, path))

    def set_auto_answer(self, value):
        return libruss.russ_svcnode_set_auto_answer(self._ptr, value)

    def set_virtual(self, value):
        libruss.russ_svcnode_set_virtual(self._ptr, value)

class Server:
    """Wrapper for russ_svr object and associated methods.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    @classmethod
    def new(cls, root, typ):
        _ptr = libruss.russ_svr_new(root._ptr, typ)
        if not bool(_ptr):
            raise Exception("could not create Server")
        return cls(_ptr)

    def accept(self, deadline):
        conn_ptr = libruss.russ_svr_accept(self._ptr, deadline)
        return bool(conn_ptr) and ServerConn(conn_ptr) or None

    def announce(self, saddr, mode, uid, gid):
        lis_ptr = libruss.russ_svr_announce(self._ptr, saddr, mode, uid, gid)
        return bool(lis_ptr) and Listener(lis_ptr) or None

    def free(self):
        libruss.russ_svr_free(self._ptr)
        self._ptr = None

    def handler(self, conn):
        libruss.russ_svr_handler(self._ptr, conn._ptr)

    def loop(self):
        libruss.russ_svr_loop(self._ptr)

    def loop_thread(self):
        def helper(svr, conn):
            self.handler(conn)
            # failsafe exit info (if not provided)
            conn.fatal(pyruss.RUSS_MSG_NO_EXIT, pyruss.RUSS_EXIT_SYS_FAILURE)
            conn.free()

        while True:
            conn = self.accept(self._ptr.contents.accept_timeout)
            if not conn:
                sys.stderr.write("error: cannot accept connection\n")
                continue
            th = threading.Thread(target=helper, args=(self, conn))
            if th:
                th.start()
            else:
                sys.stderr.write("error: cannot spawn thread\n")

class Sess:
    """Wrapper for russ_sess.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    def __del__(self):
        self._ptr = None

    def get_conn(self):
        return self._ptr.contents.conn and ServerConn(self._ptr.contents.conn) or None

    def get_svr(self):
        return self._ptr.contents.svr and Server(self._ptr.contents.svr) or None

    def get_request(self):
        return self._ptr.contents.req and Request(self._ptr.contents.req) or None