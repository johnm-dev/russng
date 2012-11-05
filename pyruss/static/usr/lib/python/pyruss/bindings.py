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

#
# C library interfaces
#

# russ.h
RUSS_CONN_NFDS = 32
RUSS_CONN_STD_NFDS = 4
RUSS_DEADLINE_NEVER = (2<<63)-1 # INT64_MAX
RUSS_MAX_ATTRC = 1024
RUSS_MAX_ARGC = 1024

RUSS_MSG_BAD_ARGS = "error: bad/missing arguments"
RUSS_MSG_BAD_OP = "error: unsupported operation"
RUSS_MSG_NO_DIAL = "error: cannot dial service"
RUSS_MSG_NO_EXIT = "error: no exit status"
RUSS_MSG_NO_SERVICE = "error: no service"
RUSS_MSG_NO_SWITCH_USER = "error: cannot switch user"
RUSS_MSG_UNDEF_SERVICE = "warning: undefined service"

RUSS_EXIT_SUCCESS = 0
RUSS_EXIT_FAILURE = 1
RUSS_EXIT_CALL_FAILURE = 126
RUSS_EXIT_SYS_FAILURE = 127

# data type descriptions
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
        ("creds", russ_credentials_Structure),
        ("req", russ_request_Structure),
        ("sd", ctypes.c_int),
        ("fds", ctypes.c_int*RUSS_CONN_NFDS),
    ]

# conn.c
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

libruss.russ_conn_await_request.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int64,  # russ_deadline
]
libruss.russ_conn_await_request.restype = ctypes.c_int

libruss.russ_conn_close.argtypes = [
    ctypes.c_void_p
]
libruss.russ_conn_close.restype = None

libruss.russ_conn_close_fd.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
]
libruss.russ_conn_close_fd.restype = ctypes.c_int

libruss.russ_conn_exit.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
]
libruss.russ_conn_exit.restype = ctypes.c_int

libruss.russ_conn_fatal.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_char_p,
    ctypes.c_int,
]
libruss.russ_conn_fatal.restype = ctypes.c_int

libruss.russ_conn_free.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
]
libruss.russ_conn_free.restype = ctypes.POINTER(russ_conn_Structure)

libruss.russ_conn_sendfds.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_void_p,
]
libruss.russ_conn_sendfds.restype = ctypes.c_int

libruss.russ_conn_splice.argtypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.POINTER(russ_conn_Structure),
]
libruss.russ_conn_splice.restype = ctypes.c_int

libruss.russ_conn_wait.argstypes = [
    ctypes.POINTER(russ_conn_Structure),
    ctypes.POINTER(ctypes.c_int),
    ctypes.c_int64,  # russ_deadline
]
libruss.russ_conn_wait.restype = ctypes.c_int

libruss.russ_dialv.argtypes = [
    ctypes.c_int64,  # russ_deadline
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
]
libruss.russ_dialv.restype = ctypes.POINTER(russ_conn_Structure)

# handlers.c
libruss.russ_standard_accept_handler.argtypes = [
    ctypes.POINTER(russ_conn_Structure)
]
libruss.russ_standard_accept_handler.restype = ctypes.c_int

libruss.russ_standard_answer_handler.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
    ctypes.c_int64,
]
libruss.russ_standard_answer_handler.restype = ctypes.POINTER(russ_conn_Structure)

# listener.c
libruss.russ_announce.argtypes = [
    ctypes.c_char_p,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.c_uint,
]
libruss.russ_announce.restype = ctypes.POINTER(russ_listener_Structure)

libruss.russ_listener_answer.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
    ctypes.c_int64,  # russ_deadline
]
libruss.russ_listener_answer.restype = ctypes.POINTER(russ_conn_Structure)

libruss.russ_listener_close.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
]
libruss.russ_listener_close.restype = None

libruss.russ_listener_free.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
]
libruss.russ_listener_free.restype = ctypes.POINTER(russ_listener_Structure)

libruss.russ_listener_loop.argtypes = [
    ctypes.POINTER(russ_listener_Structure),
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_void_p,
]
libruss.russ_listener_loop.restype = None

# misc.c
libruss.russ_switch_user.argtypes = [
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_int),
]
libruss.russ_switch_user.restype = ctypes.c_int

libruss.russ_unlink.argtypes = [
    ctypes.c_char_p,
]
libruss.russ_unlink.restype = ctypes.c_int

# from time.h
libruss.russ_gettime.argtypes = []
libruss.russ_gettime.restype = ctypes.c_int64

libruss.russ_to_deadline.argtypes = [
    ctypes.c_int,
]
libruss.russ_to_deadline.restype = ctypes.c_int64

libruss.russ_to_deadline_diff.argtypes = [
    ctypes.c_int64,
]
libruss.russ_to_deadline_diff.restype = ctypes.c_int64

libruss.russ_to_timeout.argtypes = [
    ctypes.c_int64,
]
libruss.russ_to_timeout.restype = ctypes.c_int

#
# python-based helpers
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
# Application-facing functions, classes, and more
#
def announce(path, mode, uid, gid):
    """Announce a service.
    """
    lis_ptr = libruss.russ_announce(path, mode, uid, gid)
    return lis_ptr and Listener(lis_ptr)

def dialv(deadline, op, saddr, attrs, args):
    """Dial a service.
    """
    if attrs == None:
        attrs = {}
    attrs_list = ["%s=%s" % (k, v) for k, v in attrs.items()]
    if args == None:
        args = []
    c_attrs = list_of_strings_to_c_string_array(list(attrs_list)+[None])
    c_argv = list_of_strings_to_c_string_array(list(args)+[None])
    conn_ptr = libruss.russ_dialv(deadline, op, saddr, c_attrs, c_argv)
    return conn_ptr and ClientConn(conn_ptr)

dial = dialv

def execv(deadline, saddr, attrs, args):
    """ruexec a service.
    """
    return dialv(deadline, "execute", saddr, attrs, args)

def gettime():
    return libruss.russ_gettime()

def switch_user(uid, gid, gids):
    return libruss.russ_switch_user(uid, gid, len(gids), gids)

def to_deadline(timeout):
    return libruss.russ_to_deadline(timeout)

def to_timeout(deadline):
    return libruss.russ_to_timeout(deadline)

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

    def close(self):
        libruss.russ_conn_close(self.conn_ptr)

    def close_fd(self, i):
        return libruss.russ_conn_close_fd(self.conn_ptr, i)

    def get_creds(self):
        creds = self.conn_ptr.contents.creds
        return Credentials(creds.uid, creds.gid, creds.pid)

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

    def splice(self, dconn):
        return libruss.russ_conn_splice(self.conn_ptr, dconn.conn_ptr)

class ClientConn(Conn):
    """Client connection.
    """

    def get_fd(self, i):
        return self.conn_ptr.contents.fds[i]

    def get_sd(self):
        return self.conn_ptr.contents.sd

    def wait(self, deadline):
        exit_status = ctypes.c_int()
        return libruss.russ_conn_wait(self.conn_ptr, ctypes.byref(exit_status), deadline), exit_status.value

class Credentials:
    """Connection credentials.
    """

    def __init__(self, uid, gid, pid):
        self.uid = uid
        self.gid = gid
        self.pid = pid

class ServerConn(Conn):
    """Server connection.
    """

    def accept(self, nfds, cfds, sfds):
        if 0:
            # TODO: how to handle passing fds?
            return libruss.russ_conn_accept(self.conn_ptr, nfds, ctypes.POINTER(cfds), ctypes.POINTER(sfds))
        else:
            return libruss.russ_conn_accept(self.conn_ptr, 0, None, None)

    def await_request(self, deadline):
        return libruss.russ_conn_await_request(self.conn_ptr, deadline)

    def exit(self, exit_status):
        return libruss.russ_conn_exit(self.conn_ptr, exit_status)

    def fatal(self, msg, exit_status):
        return libruss.russ_conn_fatal(self.conn_ptr, msg, exit_status)

    def standard_accept_handler(self):
        return libruss.russ_standard_accept_handler(self.conn_ptr)

REQ_HANDLER_FUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)

class Listener:
    def __init__(self, lis_ptr):
        self.lis_ptr = lis_ptr

    def __del__(self):
        libruss.russ_listener_close(self.lis_ptr)
        libruss.russ_listener_free(self.lis_ptr)
        self.lis_ptr = None

    def answer(self, deadline):
        try:
            conn_ptr = libruss.russ_listener_answer(self.lis_ptr, deadline)
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

    def _loop(self, answer_handler, accept_handler, req_handler):
        # TODO: support accept_handler
        def raw_handler(conn_ptr):
            req_handler(ServerConn(conn_ptr))
            return 0    # TODO: allow a integer return value from handler
        libruss.russ_listener_loop(self.lis_ptr, None, None, REQ_HANDLER_FUNC(raw_handler))

    def loop(self, answer_handler, accept_handler, req_handler):
        """Fork-based loop.
        """
        if answer_handler == None:
            answer_handler = Listener.standard_answer_handler
        if accept_handler == None:
            accept_handler = ServerConn.standard_accept_handler

        while self.get_sd() >= 0:
            try:
                conn = answer_handler(self, RUSS_DEADLINE_NEVER)
                if conn == None:
                    sys.stderr.write("error: cannot answer connection\n")
                    continue

                if os.fork() == 0:
                    self.close()
                    if conn.await_request(RUSS_DEADLINE_NEVER) < 0 \
                        or accept_handler(conn) < 0:
                        conn.close()
                        sys.exit(-1)
                    try:
                        req_handler(conn)
                    except:
                        pass
                    try:
                        if conn:
                            conn.fatal(RUSS_MSG_NO_EXIT, RUSS_EXIT_FAILURE)
                            conn.close()
                    except:
                        pass
                    sys.exit(0)
                conn.close()
                del conn
            except SystemExit:
                pass
            except:
                #traceback.print_exc()
                pass

    def loop_thread(self, answer_handler, accept_handler, req_handler):
        """Thread-based loop.
        """
        def pre_handler_thread(conn, req_handler):
            if conn.await_request(RUSS_DEADLINE_NEVER) < 0 \
                or accept_handler(conn) < 0:
                return
            try:
                req_handler(conn)
            except:
                pass
            try:
                if conn:
                    conn.fatal(RUSS_MSG_NO_EXIT, RUSS_EXIT_FAILURE)
                    conn.close()
            except:
                pass

        if answer_handler == None:
            answer_handler = Listener.standard_answer_handler
        if accept_handler == None:
            accept_handler = ServerConn.standard_accept_handler

        while True:
            try:
                conn = answer_handler(self, RUSS_DEADLINE_NEVER)
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

    def standard_answer_handler(self, deadline):
        try:
            conn_ptr = libruss.russ_standard_answer_handler(self.lis_ptr, deadline)
        except:
            traceback.print_exc()
        return conn_ptr and ServerConn(conn_ptr)