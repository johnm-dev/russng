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
import traceback

libruss = ctypes.cdll.LoadLibrary("libruss.so")

RUSS_CONN_NFDS = 4

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
        ("nfds", ctypes.c_int),
        ("fds", ctypes.c_int*RUSS_CONN_NFDS),
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
libruss.russ_dialv.restype = ctypes.POINTER(russ_conn_Structure)

# russ_conn_accept
libruss.russ_conn_accept.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_void_p,
    # TODO: how to handle passing fds?
    #ctypes.c_int*4,
    #ctypes.c_int*4,
]
libruss.russ_conn_accept.restype = ctypes.c_int

# russ_conn_await_request
libruss.russ_conn_await_request.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
]
libruss.russ_conn_await_request.restype = ctypes.c_int

# russ_conn_close
libruss.russ_conn_close.argtypes = [ctypes.c_void_p]
libruss.russ_conn_close.restype = None

# russ_conn_close_fd
libruss.russ_conn_close_fd.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
]
libruss.russ_conn_close_fd.restype = ctypes.c_int

# russ_conn_exit
libruss.russ_conn_exit.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
]
libruss.russ_conn_exit.restype = ctypes.c_int

# russ_conn_fatal
libruss.russ_conn_fatal.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_char_p,
    ctypes.c_int,
]
libruss.russ_conn_fatal.restype = ctypes.c_int

# russ_conn_free
libruss.russ_conn_free_argtypes = [
    ctypes.POINTER(russ_conn_Structure),
]
libruss.russ_conn_free.restype = ctypes.POINTER(russ_conn_Structure)

# russ_conn_wait
libruss.russ_conn_wait.argstypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.POINTER(ctypes.c_int),
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
libruss.russ_announce.restype = ctypes.POINTER(russ_listener_Structure)

# russ_unlink
libruss.russ_unlink.argtypes = [
    ctypes.c_char_p,
]
libruss.russ_unlink.restype = ctypes.c_int

# russ_listener_answer
libruss.russ_listener_answer.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
    ctypes.c_int64,  # russ_timeout
]
libruss.russ_listener_answer.restype = ctypes.POINTER(russ_conn_Structure)

# russ_listener_close
libruss.russ_listener_close.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
]
libruss.russ_listener_close.restype = None

# russ_listener_free
libruss.russ_listener_free.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
]
libruss.russ_listener_free.restype = ctypes.POINTER(russ_listener_Structure)

# russ_loop
libruss.russ_listener_loop.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
    ctypes.c_void_p,
]
libruss.russ_listener_loop.restype = None

#
# helpers
#
def list_of_strings_to_c_string_array(l):
    """Create "char **" from list of strings. All None elements are
    converted to NULL.
    """
    c_strings = (ctypes.c_char_p*(len(l)))()
    for i, s in enumerate(l):
        if s == None:
            c_strings[i] = None
        else:
            c_strings[i] = ctypes.create_string_buffer(s).value
    return c_strings
        
#
# Application-facing classes
#
def announce(path, mode, uid, gid):
    """Announce a service.
    """
    lis_ptr = libruss.russ_announce(path, mode, uid, gid)
    return lis_ptr and Listener(lis_ptr)

def dialv(timeout, op, saddr, attrs, args):
    """Dial a service.
    """
    if attrs == None:
        attrs = {}
    attrs_list = ["%s=%s" % (k, v) for k, v in attrs.items()]

    c_attrs = list_of_strings_to_c_string_array(list(attrs_list)+[None])
    c_argv = list_of_strings_to_c_string_array(list(args)+[None])

    return ClientConn(libruss.russ_dialv(timeout, op, saddr, c_attrs, c_argv))

dial = dialv

def execv(timeout, saddr, attrs, args):
    """ruexec a service.
    """
    return dialv(timeout, "execute", saddr, attrs, args)

def unlink(path):
    """Unlink service path.
    """
    return libruss.russ_unlink(path)

class Conn:
    """Common (client, server) connection.
    """

    def __init__(self, conn_ptr):
        self.conn_ptr = conn_ptr

    def __del__(self):
        libruss.russ_conn_free(self.conn_ptr)
        self.conn_ptr = None

    def close_fd(self, i):
        return libruss.russ_close_fd(self.conn_ptr, i)

    def get_cred(self):
        cred = self.conn_ptr.contents.cred
        return (cred.pid, cred.uid, cred.gid)

    def get_fd(self, i):
        return self.conn_ptr.contents.fds[i]

    def get_request(self):
        return self.conn_ptr.contents.req

    def get_request_args(self):
        req = self.conn_ptr.contents.req
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
        req = self.conn_ptr.contents.req
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
        return self.conn_ptr.contents.sd

    def close(self):
        libruss.russ_conn_close(self.conn_ptr)

class ClientConn(Conn):
    """Client connection.
    """

    def get_fd(self, i):
        return self.conn_ptr.contents.fds[i]

    def get_sd(self):
        return self.conn_ptr.contents.sd

    def wait(self, timeout):
        exit_status = ctypes.c_int()
        return libruss.russ_conn_wait(self.conn_ptr, ctypes.byref(exit_status), timeout), exit_status.value

class ServerConn(Conn):
    """Server connection.
    """

    def accept(self, nfds, cfds, sfds):
        if 0:
            # TODO: how to handle passing fds?
            return libruss.russ_conn_accept(self.conn_ptr, nfds, ctypes.POINTER(cfds), ctypes.POINTER(sfds))
        else:
            return libruss.russ_conn_accept(self.conn_ptr, 0, None, None)

    def await_request(self):
        return libruss.russ_conn_await_request(self.conn_ptr)

    def exit(self, exit_status):
        return libruss.russ_conn_exit(self.conn_ptr, exit_status)

    def fatal(self, msg, exit_status):
        return libruss.russ_conn_fatal(self.conn_ptr, msg, exit_status)

HANDLERFUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)

class Listener:
    def __init__(self, lis_ptr):
        self.lis_ptr = lis_ptr

    def __del__(self):
        libruss.russ_listener_close(self.lis_ptr)
        libruss.russ_listener_free(self.lis_ptr)
        self.lis_ptr = None

    def answer(self, timeout):
        try:
            conn_ptr = libruss.russ_listener_answer(self.lis_ptr, timeout)
        except:
            traceback.print_exc()
        return conn_ptr and ServerConn(conn_ptr)

    def close(self):
        libruss.russ_listener_close(self.lis_ptr)

    def get_sd(self):
        if self.lis_ptr:
            return self.lis_ptr.contents.sd
        else:
            return -1

    def _loop(self, accept_handler, req_handler):
        # TODO: support accept_handler
        def raw_handler(conn_ptr):
            req_handler(ServerConn(conn_ptr))
            return 0    # TODO: allow a integer return value from handler
        libruss.russ_listener_loop(self.lis_ptr, None, HANDLERFUNC(raw_handler))

    def loop(self, accept_handler, req_handler):
        """Fork-based loop.
        """
        if accept_handler:
            raise Exception("error: accept_handler not supported")

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
                    req_handler(conn)
                    os.exit(0)
                conn.close()
                del conn
            except SystemExit:
                pass
            except:
                #traceback.print_exc()
                pass

    def loop_thread(self, accept_handler, req_handler):
        """Thread-based loop.
        """
        def pre_handler_thread(conn, req_handler):
            if conn.await_request() < 0 \
                or conn.accept(None, None) < 0:
                return
            req_handler(conn)

        if accept_handler:
            raise Exception("error: accept_handler not supported")

        while True:
            try:
                conn = self.answer(-1)
                if conn == None:
                    sys.stderr.write("error: cannot answer connection\n")
                    continue
                # no limiting of thread count
                Thread(target=pre_handler_thread, args=(conn, req_handler)).start()
            except SystemExit:
                pass
            except:
                #traceback.print_exc()
                pass
