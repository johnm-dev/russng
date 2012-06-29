#! /usr/bin/env python
#
# pyruss/bindings.py

# license--start
#
#  This file is part of the pyruss library.
#  Copyright (C) 2012 John Marshall
#
#  The RUSS library is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end

"""Python bindings to the russ library C API.
"""

import ctypes
import os
import sys
from threading import Thread

libruss = ctypes.cdll.LoadLibrary("libruss.so")

#
# data type descriptions
#
class russ_credentials_Structure(ctypes.Structure):
    _fields_ = [
        ("pid", ctypes.c_long),
        ("uid", ctypes.c_long),
        ("gid", ctypes.c_long),
    ]

class russ_listener_Structure(ctypes.Structure):
    _fields_ = [
        ("sd", ctypes.c_int),
    ]

class russ_request_Structure(ctypes.Structure):
    _fields_ = [
        ("protocol_string", ctypes.c_char_p),
        ("op", ctypes.c_char_p),
        ("spath", ctypes.c_char_p),
        ("attrv", ctypes.POINTER(ctypes.c_char_p)),
        ("argv", ctypes.POINTER(ctypes.c_char_p)),
    ]

class russ_conn_Structure(ctypes.Structure):
    _fields_ = [
        ("conn_type", ctypes.c_int),
        ("cred", russ_credentials_Structure),
        ("req", russ_request_Structure),
        ("sd", ctypes.c_int),
        ("exit_fd", ctypes.c_int),
        ("nfds", ctypes.c_int),
        ("fds", ctypes.c_int*3),
    ]

#
# C library interfaces
#

# russ_dialv
libruss.russ_dialv.argtypes = [
    ctypes.c_int64,  # russ_timeout
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
]
libruss.russ_dialv.restype = ctypes.c_void_p

# russ_conn_accept
libruss.russ_conn_accept.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_void_p,
    # TODO: how to handle passing fds?
    #ctypes.c_int*4,
    #ctypes.c_int*4,
]
libruss.russ_conn_accept.restype = ctypes.c_int

# russ_conn_await_request
libruss.russ_conn_await_request.argtypes = [
    ctypes.c_void_p,
]
libruss.russ_conn_await_request.restype = ctypes.c_int

# russ_conn_close
libruss.russ_conn_close.argtypes = [ctypes.c_void_p]
libruss.russ_conn_close.restype = None

# russ_conn_exit
libruss.russ_conn_exit.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_char_p,
]
libruss.russ_conn_exit.restype = ctypes.c_int

# russ_conn_free
libruss.russ_conn_free_argtypes = [
    ctypes.c_void_p,
]
libruss.russ_conn_free.restype = None

# russ_conn_wait
libruss.russ_conn_wait.argstypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.c_int64,  # russ_timeout
]
libruss.russ_conn_wait.restype = ctypes.c_int

# russ_announce
libruss.russ_announce.argtypes = [
    ctypes.c_char_p,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.c_uint,
]
libruss.russ_announce.restype = ctypes.c_void_p

# russ_listener_answer
libruss.russ_listener_answer.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int64,  # russ_timeout
]
libruss.russ_listener_answer.restype = ctypes.c_void_p

# russ_listener_close
libruss.russ_listener_close.argtypes = [
    ctypes.c_void_p,
]
libruss.russ_listener_close.restype = None

# russ_listener_free
libruss.russ_listener_free.argtypes = [
    ctypes.c_void_p,
]
libruss.russ_listener_free.restype = ctypes.c_void_p

# russ_loop
libruss.russ_listener_loop.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
]
libruss.russ_listener_loop.restype = None

#
# Application-facing classes
#
def dial(timeout, op, saddr, attrs, args):
    """Dial a service.
    """
    if attrs == None:
        attrs = {}
    attrs_list = ["%s=%s" % (k, v) for k, v in attrs.items()]
    c_attrs = (ctypes.c_char_p*(len(attrs_list)+1))(*attrs_list)
    c_attrs[len(attrs)] = None
    c_argv = (ctypes.c_char_p*(len(args)+1))(*args)
    c_argv[len(args)] = None
    return ClientConn(libruss.russ_dialv(timeout, op, saddr, c_attrs, c_argv))

def announce(path, mode, uid, gid):
    """Announce a service.
    """
    return Listener(libruss.russ_announce(path, mode, uid, gid))

class Conn:
    """Common (client, server) connection.
    """

    def __init__(self, raw_conn):
        self.raw_conn = raw_conn
        self.ptr_conn = ctypes.cast(raw_conn, ctypes.POINTER(russ_conn_Structure))

    def __del__(self):
        libruss.russ_conn_free(self.raw_conn)
        self.raw_conn = None
        self.ptr_conn = None

    def close_fd(self, i):
        return libruss.russ_close_fds(i, self.raw_conn.fds)

    def get_cred(self):
        cred = self.ptr_conn.contents.cred
        return (cred.pid, cred.uid, cred.gid)

    def get_fd(self, i):
        return self.ptr_conn.contents.fds[i]

    def get_request(self):
        return self.ptr_conn.contents.req

    def get_request_args(self):
        req = self.ptr_conn.contents.req
        args = []
        if bool(req.argv):
            i = 0
            while 1:
                s = req.argv[i]
                i += 1
                if s == None:
                    break
                args.append(s)
        return  args

    def get_request_attrs(self):
        req = self.ptr_conn.contents.req
        attrs = {}
        if bool(req.attrv):
            i = 0
            while 1:
                s = req.attrv[i]
                i += 1
                if s == None:
                    break
                try:
                    k, v = s.split("=", 1)
                    attrs[k] = v
                except:
                    pass
        return attrs

    def get_sd(self):
        return self.ptr_conn.contents.sd

    def close(self):
        libruss.russ_conn_close(self.raw_conn)

class ClientConn(Conn):
    """Client connection.
    """

    def get_fd(self, i):
        return self.ptr_conn.contents.fds[i]

    def get_sd(self):
        return self.ptr_conn.contents.sd

    def wait(self, timeout):
        exit_status = c_int()
        return libruss.russ_conn_wait(self.raw_conn, ctypes.byref(exit_status), timeout), exit_status

class ServerConn(Conn):
    """Server connection.
    """

    def accept(self, cfds, sfds):
        if 0:
            # TODO: how to handle passing fds?
            return libruss.russ_conn_accept(self.raw_conn, ctypes.POINTER(cfds), ctypes.POINTER(sfds))
        else:
            return libruss.russ_conn_accept(self.raw_conn, None, None)

    def await_request(self):
        return libruss.russ_conn_await_request(self.raw_conn)

    def exit(self, exit_status):
        return libruss.russ_conn_exit(self.raw_conn, exit_status)

HANDLERFUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)

class Listener:
    def __init__(self, raw_lis):
        self.raw_lis = raw_lis
        self.ptr_lis = ctypes.cast(raw_lis, ctypes.POINTER(russ_listener_Structure))

    def __del__(self):
        if self.raw_lis != None:
            libruss.russ_listener_close(self.raw_lis)
            libruss.russ_listener_free(self.raw_lis)
            self.raw_lis = None
            self.ptr_lis= None

    def answer(self, timeout):
        raw_conn = libruss.russ_listener_answer(self.raw_lis, timeout)
        return raw_conn and ServerConn(raw_conn)

    def close(self):
        libruss.russ_listener_close(self.raw_lis)

    def get_sd(self):
        if self.raw_lis:
            return self.ptr_lis.contents.sd
        else:
            return -1

    def _loop(self, handler):
        def raw_handler(raw_conn):
            handler(ServerConn(raw_conn))
            return 0    # TODO: allow a integer return value from handler
        libruss.russ_listener_loop(self.raw_lis, HANDLERFUNC(raw_handler))

    def loop(self, handler):
        """Fork-based loop.
        """
        while self.get_sd() >= 0:
            try:
                conn = self.answer(-1)
                if conn == None:
                    sys.stderr.write("error: cannot answer connection\n")
                    continue
                if os.fork() == 0:
                    self.close()
                    if conn.await_request() < 0 \
                        or conn.accept(None, None) < 0:
                        os.exit(-1)
                    handler(conn)
                    os.exit(0)
                conn.close()
                del conn
            except SystemExit:
                pass
            except:
                #traceback.print_exc()
                pass

    def loop_thread(self, handler):
        """Thread-based loop.
        """
        def pre_handler_thread(conn, handler):
            if conn.await_request() < 0 \
                or conn.accept(None, None) < 0:
                return
            handler(conn)

        while True:
            try:
                conn = self.answer(-1)
                if conn == None:
                    sys.stderr.write("error: cannot answer connection\n")
                    continue
                # no limiting of thread count
                Thread(target=pre_handler_thread, args=(conn, handler)).start()
            except SystemExit:
                pass
            except:
                #traceback.print_exc()
                pass

