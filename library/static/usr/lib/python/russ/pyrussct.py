#! /usr/bin/env python
#
# pyrussct.py

# license--start
#
#  This file is part of the RUSS library.
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

"""Preliminary interface to the russ library.
"""

import ctypes

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

class russ_req_Structure(ctypes.Structure):
    _fields_ = [
        ("protocol_string", ctypes.c_char_p),
        ("spath", ctypes.c_char_p),
        ("op", ctypes.c_char_p),
        ("argc", ctypes.c_int),
        ("argv", ctypes.POINTER(ctypes.c_char_p)),
    ]

class russ_conn_Structure(ctypes.Structure):
    _fields_ = [
        ("conn_type", ctypes.c_int),
        ("cred", russ_credentials_Structure),
        ("req", ctypes.c_void_p),
        ("sd", ctypes.c_int),
        ("fds", ctypes.c_int*3),
    ]

#
# C library interfaces
#
libruss.russ_dialv.argtypes = [
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_char_p),
]
libruss.russ_dialv.restype = ctypes.c_void_p

libruss.russ_close_conn.argtypes = [ctypes.c_void_p]
libruss.russ_close_conn.restype = None

libruss.russ_free_conn_argtypes = [
    ctypes.c_void_p,
]
libruss.russ_free_conn.restype = None

libruss.russ_announce.argtypes = [
    ctypes.c_char_p,
    ctypes.c_uint,
    ctypes.c_uint,
    ctypes.c_uint,
]
libruss.russ_announce.restype = ctypes.c_void_p

libruss.russ_answer.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
]
libruss.russ_answer.restype = ctypes.c_void_p

libruss.russ_accept.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int*3,
    ctypes.c_int*3,
]
libruss.russ_accept.restype = ctypes.c_void_p

libruss.russ_close_listener.argtypes = [
    ctypes.c_void_p,
]
libruss.russ_close_listener.restype = None

libruss.russ_free_listener.argtypes = [
    ctypes.c_void_p,
]
libruss.russ_free_listener.restype = ctypes.c_void_p

libruss.russ_loop.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
]
libruss.russ_loop.restype = None

#
# Application-facing classes
#
def dial(saddr, op, timeout, args):
    """Dial a service.
    """
    argv = (ctypes.c_char_p*len(args))(*args)
    return ClientConn(libruss.russ_dialv(saddr, op, timeout, len(args), argv))

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
        libruss.russ_free_conn(self.raw_conn)
        self.raw_conn = None
        self.ptr_conn = None

    def close_fd(self, i):
        return libruss.russ_close_fds(i, self.raw_conn.fds)

    def get_fd(self, i):
        return self.ptr_conn.contents.fds[i]

    def get_sd(self):
        return self.ptr_conn.contents.sd

    def close(self):
        libruss.russ_close_conn(self.raw_conn)

class ClientConn(Conn):
    """Client connection.
    """

    def get_fd(self, i):
        return self.ptr_conn.contents.fds[i]

    def get_sd(self):
        return self.ptr_conn.contents.sd

class ServerConn(Conn):
    """Server connection.
    """

    def accept(self, cfds, sfds):
        libruss.russ_accept(self.raw_conn, ctypes.POINTER(cfds), ctypes.POINTER(sfds))

    def await_request(self):
        libruss.russ_await_request(self.raw_conn)

    def get_request(self):
        return PyRussRequest(self.ptr_conn.contents.req)

HANDLERFUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)

class Listener:
    def __init__(self, raw_lis):
        self.raw_lis = raw_lis

    def __del__(self):
        self.raw_conn = libruss.russ_free_listener(self.raw_lis)

    def answer(self, timeout):
        return ServerConn(libruss.russ_answer(self.raw_lis, timeout))

    def close(self):
        libruss.russ_close_listener(self.raw_lis)

    def loop(self, handler):
        def raw_handler(raw_conn):
            return handler(ServerConn(raw_conn))
        libruss.russ_loop(self.raw_lis, HANDLERFUNC(raw_handler))