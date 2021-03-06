#! /usr/bin/env python3
#! /usr/bin/env python2
#
# pyruss/bindings.py

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

"""Python bindings to the russ library C API.

* ctypes handles conversion of Python integer to russ_deadline type
  (e.g., in dialv())
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

# russ/conf.h
RUSS_CONFFILE_MARKER_FMT = "%5s"
RUSS_CONFFILE_MARKER_STR = "#russ"

# russ/russ.h
RUSS_CONN_NFDS = 32
RUSS_CONN_STD_NFDS = 3
RUSS_CONN_FD_STDIN = 0
RUSS_CONN_FD_STDOUT = 1
RUSS_CONN_FD_STDERR = 2

RUSS_CONN_NSYSFDS = 1
RUSS_CONN_FD_EXIT = 0

RUSS_CONN_MAX_NFDS = max(RUSS_CONN_NFDS, RUSS_CONN_NSYSFDS)

RUSS_DEADLINE_NEVER = (1<<63)-1 # INT64_MAX

RUSS_EXIT_SUCCESS = 0
RUSS_EXIT_FAILURE = 1
RUSS_EXIT_EXITFDCLOSED = 125
RUSS_EXIT_CALLFAILURE = 126
RUSS_EXIT_SYSFAILURE = 127

RUSS_MSG_BADARGS = "error: bad/missing arguments"
RUSS_MSG_BADCONNEVENT = "error: unexpected connection event"
RUSS_MSG_BADOP = "error: unsupported operation"
RUSS_MSG_BADSITUATION = "error: unexpected situation"
RUSS_MSG_NOACCESS = "error: insufficient privilege"
RUSS_MSG_NODIAL = "error: cannot dial service"
RUSS_MSG_NOEXIT = "error: no exit status"
RUSS_MSG_NOLIST = "info: list not available"
RUSS_MSG_NOSERVICE = "error: no service"
RUSS_MSG_NOSWITCHUSER = "error: cannot switch user"
RUSS_MSG_UNDEFSERVICE = "warning: undefined service"
RUSS_MSG_BADUSER = "error: bad user"

RUSS_OPNUM_NOTSET = 0
RUSS_OPNUM_EXTENSION = 1
RUSS_OPNUM_EXECUTE = 2
RUSS_OPNUM_HELP = 3
RUSS_OPNUM_ID = 4
RUSS_OPNUM_INFO = 5
RUSS_OPNUM_LIST = 6

RUSS_REQ_ARGS_MAX = 1024
RUSS_REQ_ATTRS_MAX = 1024
RUSS_REQ_SPATH_MAX = 8192
RUSS_REQ_PROTOCOLSTRING = "0010"

RUSS_SVR_LIS_SD_DEFAULT = 3
RUSS_SVR_TIMEOUT_ACCEPT = (1<<31)-1 # INT32_MAX
RUSS_SVR_TIMEOUT_AWAIT = 15000
RUSS_SVR_TYPE_FORK = 1
RUSS_SVR_TYPE_THREAD = 2

RUSS_WAIT_UNSET = 1
RUSS_WAIT_OK = 0
RUSS_WAIT_FAILURE = -1
RUSS_WAIT_BADFD = -2
RUSS_WAIT_TIMEOUT = -3
RUSS_WAIT_HUP = -4

# typedef aliases
russ_deadline = ctypes.c_int64
russ_opnum = ctypes.c_uint32

# helper converters for converting between string and bytes; to be
# for all interactions with C calls (vis libruss functions)
# rules:
# * use strtobytes() when setting at call site
# * use bytestostr() when getting at call site
# * do not convert too early
# * do not convert when calling python code
if sys.version_info[0] == 3:
    def strtobytes(s):
        if s == None:
            return s
        return s.encode()

    def bytestostr(b):
        if b == None:
            return b
        return b.decode()
else:
    # effectively, noops
    def strtobytes(s):
        return s

    def bytestostr(b):
        return b

# data type descriptions
class russ_buf_Structure(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.c_void_p),
        ("cap", ctypes.c_int),
        ("len", ctypes.c_int),
        ("off", ctypes.c_int),
    ]

class russ_creds_Structure(ctypes.Structure):
    _fields_ = [
        ("pid", ctypes.c_long),
        ("uid", ctypes.c_long),
        ("gid", ctypes.c_long),
    ]

class russ_req_Structure(ctypes.Structure):
    _fields_ = [
        ("protocolstring", ctypes.c_char_p),
        ("op", ctypes.c_char_p),
        ("opnum", russ_opnum),
        ("spath", ctypes.c_char_p),
        ("attrv", ctypes.POINTER(ctypes.c_char_p)),
        ("argv", ctypes.POINTER(ctypes.c_char_p)),
    ]

class russ_cconn_Structure(ctypes.Structure):
    _fields_ = [
        ("sd", ctypes.c_int),
        ("fds", ctypes.c_int*RUSS_CONN_NFDS),
        ("sysfds", ctypes.c_int*RUSS_CONN_NSYSFDS),
        ("nevbuf", ctypes.c_int),
        ("evbuf", ctypes.c_char*4),
    ]

class russ_sconn_Structure(ctypes.Structure):
    _fields_ = [
        ("creds", russ_creds_Structure),
        ("sd", ctypes.c_int),
        ("fds", ctypes.c_int*RUSS_CONN_NFDS),
        ("sysfds", ctypes.c_int*RUSS_CONN_NSYSFDS),
    ]

class russ_sess_Structure(ctypes.Structure):
    pass
SVCHANDLER_FUNC = ctypes.CFUNCTYPE(None, ctypes.POINTER(russ_sess_Structure))

class russ_svcnode_Structure(ctypes.Structure):
    pass
russ_svcnode_Structure._fields_ = [
        ("handler", SVCHANDLER_FUNC),
        ("name", ctypes.c_char_p),
        ("next", ctypes.POINTER(russ_svcnode_Structure)),
        ("children", ctypes.POINTER(russ_svcnode_Structure)),
        ("autoanswer", ctypes.c_int),
        ("virtual", ctypes.c_int),
        ("wildcard", ctypes.c_int),
    ]

class russ_svr_Structure(ctypes.Structure):
    _fields_ = [
        ("root", ctypes.POINTER(russ_svcnode_Structure)),
        ("type", ctypes.c_int),
        ("mpid", ctypes.c_int),
        ("ctime", ctypes.c_long),
        ("saddr", ctypes.c_char_p),
        ("lisd", ctypes.c_int),
        ("closeonaccept", ctypes.c_int),
        ("accepthandler", ctypes.c_void_p), # cheating to get pointer
        ("accepttimeout", ctypes.c_int),
        ("allowrootuser", ctypes.c_int),
        ("answerhandler", ctypes.c_void_p), # cheating to get pointer
        ("awaittimeout", ctypes.c_int),
        ("autoswitchuser", ctypes.c_int),
        ("matchclientuser", ctypes.c_int),
        ("help", ctypes.c_char_p),
    ]

russ_sess_Structure._fields_ = [
        ("svr", ctypes.POINTER(russ_svr_Structure)),
        ("sconn", ctypes.POINTER(russ_sconn_Structure)),
        ("req", ctypes.POINTER(russ_req_Structure)),
        ("spath", ctypes.c_char_p),
        ("name", ctypes.c_char_p),
        ("options", ctypes.POINTER(ctypes.c_char_p)),
    ]

#
# from buf.c
#
libruss.russ_buf_init.argtypes = [
    ctypes.POINTER(russ_buf_Structure),
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
]
libruss.russ_buf_init.restype = ctypes.c_int

libruss.russ_buf_new.argtypes = [
    ctypes.c_int,
]
libruss.russ_buf_new.restype = ctypes.POINTER(russ_buf_Structure)

libruss.russ_buf_free.argtypes = [
    ctypes.POINTER(russ_buf_Structure),
]
libruss.russ_buf_free.restype = ctypes.POINTER(russ_buf_Structure)

libruss.russ_buf_set.argtypes = [
    ctypes.POINTER(russ_buf_Structure),
    ctypes.c_void_p,
    ctypes.c_int,
]
libruss.russ_buf_set.restype = ctypes.c_int

#
# from cconn.c
#
libruss.russ_cconn_free.argtypes = [
    ctypes.POINTER(russ_cconn_Structure),
]
libruss.russ_cconn_free.restype = ctypes.POINTER(russ_cconn_Structure)

libruss.russ_cconn_close.argtypes = [
    ctypes.c_void_p
]
libruss.russ_cconn_close.restype = None

libruss.russ_cconn_close_fd.argtypes = [
    ctypes.POINTER(russ_cconn_Structure),
    ctypes.c_int,
]
libruss.russ_cconn_close_fd.restype = ctypes.c_int

libruss.russ_cconn_wait.argtypes = [
    ctypes.POINTER(russ_cconn_Structure),
    russ_deadline,
    ctypes.POINTER(ctypes.c_int),
]
libruss.russ_cconn_wait.restype = ctypes.c_int

libruss.russ_dialv.argtypes = [
    russ_deadline,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
]
libruss.russ_dialv.restype = ctypes.POINTER(russ_cconn_Structure)

#
# from convenience.c
#
libruss.russ_dialv_wait.argtypes = [
    russ_deadline,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_int),
]
libruss.russ_dialv_wait.restype = ctypes.c_int

libruss.russ_dialv_wait_inouterr3.argtypes = [
    russ_deadline,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(russ_buf_Structure),
    ctypes.POINTER(russ_buf_Structure),
    ctypes.POINTER(russ_buf_Structure),
]
libruss.russ_dialv_wait_inouterr3.restype = ctypes.c_int

#
# from misc.c
#
libruss.russ_write_exit.argtypes = [
    ctypes.c_int,
    ctypes.c_int,
]
libruss.russ_write_exit.restype = ctypes.c_int

libruss.russ_get_services_dir.argtypes = [
    ctypes.c_void_p
]
libruss.russ_get_services_dir.restype = ctypes.c_char_p

#
# from optable.c
#
libruss.russ_optable_find_opnum.argtypes = [
    ctypes.c_void_p,    # pass None for default
    ctypes.c_char_p,
]
libruss.russ_optable_find_opnum.restype = russ_opnum

#
# from relay.c
#
libruss.russ_relay_loop.argtypes = [
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_int),
    ctypes.c_int,
]
libruss.russ_relay_loop.restype = ctypes.c_int

#
# from sconn.c
#
libruss.russ_sconn_accept.argtypes = [
    russ_deadline,
    ctypes.c_int,
]
libruss.russ_sconn_accept.restype = ctypes.POINTER(russ_sconn_Structure)

libruss.russ_sconn_answer.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_int),
]
libruss.russ_sconn_answer.restype = ctypes.c_int

libruss.russ_sconn_answerhandler.argtypes = [
    ctypes.POINTER(russ_sconn_Structure)
]
libruss.russ_sconn_answerhandler.restype = ctypes.c_int

libruss.russ_sconn_await_req.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    russ_deadline,
]
libruss.russ_sconn_await_req.restype = ctypes.POINTER(russ_req_Structure)

libruss.russ_sconn_close.argtypes = [
    ctypes.c_void_p
]
libruss.russ_sconn_close.restype = None

libruss.russ_sconn_close_fd.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.c_int,
]
libruss.russ_sconn_close_fd.restype = ctypes.c_int

libruss.russ_sconn_exit.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.c_int,
]
libruss.russ_sconn_exit.restype = ctypes.c_int

libruss.russ_sconn_fatal.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.c_char_p,
    ctypes.c_int,
]
libruss.russ_sconn_fatal.restype = ctypes.c_int

libruss.russ_sconn_free.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
]
libruss.russ_sconn_free.restype = ctypes.POINTER(russ_sconn_Structure)

libruss.russ_sconn_redialandsplice.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    russ_deadline,
    ctypes.POINTER(russ_req_Structure),
]
libruss.russ_sconn_redialandsplice.restype = ctypes.c_int

libruss.russ_sconn_send_fds.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_void_p,
]
libruss.russ_sconn_send_fds.restype = ctypes.c_int

libruss.russ_sconn_splice.argtypes = [
    ctypes.POINTER(russ_sconn_Structure),
    ctypes.POINTER(russ_cconn_Structure),
]
libruss.russ_sconn_splice.restype = ctypes.c_int

#
# from socket.c
#
libruss.russ_unlink.argtypes = [
    ctypes.c_char_p,
]
libruss.russ_unlink.restype = ctypes.c_int

#
# from svcnode.c
#
libruss.russ_svcnode_new.argtypes = [
    ctypes.c_char_p,
    SVCHANDLER_FUNC,
]
libruss.russ_svcnode_new.restype = ctypes.POINTER(russ_svcnode_Structure)

libruss.russ_svcnode_free.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
]
libruss.russ_svcnode_free.restype = ctypes.POINTER(russ_svcnode_Structure)

libruss.russ_svcnode_add.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_char_p,
    SVCHANDLER_FUNC,
]
libruss.russ_svcnode_add.restype = ctypes.POINTER(russ_svcnode_Structure)

libruss.russ_svcnode_find.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_char_p,
]
libruss.russ_svcnode_find.restype = ctypes.POINTER(russ_svcnode_Structure)

libruss.russ_svcnode_set_autoanswer.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_int,
]
libruss.russ_svcnode_set_autoanswer.restype = ctypes.c_int

libruss.russ_svcnode_set_virtual.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_int,
]
libruss.russ_svcnode_set_virtual.restype = ctypes.c_int

libruss.russ_svcnode_set_wildcard.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_int,
]
libruss.russ_svcnode_set_wildcard.restype = ctypes.c_int

#
# from svr.c
#
libruss.russ_svr_new.argtypes = [
    ctypes.POINTER(russ_svcnode_Structure),
    ctypes.c_int,
    ctypes.c_int,
]
libruss.russ_svr_new.restype = ctypes.POINTER(russ_svr_Structure)

libruss.russ_svr_accept.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    russ_deadline,
]
libruss.russ_svr_accept.restype = ctypes.POINTER(russ_sconn_Structure)

libruss.russ_svr_handler.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.POINTER(russ_sconn_Structure),
]
libruss.russ_svr_handler.restype = None

libruss.russ_svr_set_accepttimeout.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_accepttimeout.restype = ctypes.c_int

libruss.russ_svr_set_allowrootuser.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_allowrootuser.restype = ctypes.c_int

libruss.russ_svr_set_autoswitchuser.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_autoswitchuser.restype = ctypes.c_int

libruss.russ_svr_set_closeonaccept.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_closeonaccept.restype = ctypes.c_int

libruss.russ_svr_set_help.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_char_p,
]
libruss.russ_svr_set_help.restype = ctypes.c_int

libruss.russ_svr_set_lisd.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_lisd.restype = ctypes.c_int

libruss.russ_svr_set_matchclientuser.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_matchclientuser.restype = ctypes.c_int

libruss.russ_svr_set_root.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.POINTER(russ_svcnode_Structure),
]
libruss.russ_svr_set_root.restype = ctypes.c_int

libruss.russ_svr_set_type.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
    ctypes.c_int,
]
libruss.russ_svr_set_type.restype = ctypes.c_int

libruss.russ_svr_loop.argtypes = [
    ctypes.POINTER(russ_svr_Structure),
]
libruss.russ_svr_loop.restype = None

#
# from time.h
#
libruss.russ_gettime.argtypes = []
libruss.russ_gettime.restype = russ_deadline

libruss.russ_to_deadline.argtypes = [
    ctypes.c_int,
]
libruss.russ_to_deadline.restype = russ_deadline

libruss.russ_to_deadlinediff.argtypes = [
    russ_deadline,
]
libruss.russ_to_deadlinediff.restype = russ_deadline

libruss.russ_to_timeout.argtypes = [
    russ_deadline,
]
libruss.russ_to_timeout.restype = ctypes.c_int

#
# from user.c
#
libruss.russ_switch_user.argtypes = [
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_int),
]
libruss.russ_switch_user.restype = ctypes.c_int

libruss.russ_switch_userinitgroups.argtypes = [
    ctypes.c_int,
    ctypes.c_int,
]
libruss.russ_switch_userinitgroups.restype = ctypes.c_int
