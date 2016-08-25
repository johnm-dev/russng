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

"""Server and support for RUSS-based services.
"""

# system imports
try:
    from ConfigParser import ConfigParser as _ConfigParser
except:
    from configparser import ConfigParser as _ConfigParser
import os
import sys

#
from pyruss.bindings import *

# constants
STDOUT_SIZE_DEFAULT = 1<<20
STDERR_SIZE_DEFAULT = 1<<18

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

def convert_dial_attrs_args(attrs, args):
    """Convert attrs and args to C string arrays.
    """
    if attrs == None:
        attrs = {}
    attrs_list = ["%s=%s" % (k, v) for k, v in attrs.items()]
    if args == None:
        args = []
    c_attrs = list_of_strings_to_c_string_array(list(attrs_list)+[None])
    c_argv = list_of_strings_to_c_string_array(list(args)+[None])
    return c_attrs, c_argv

#
# Application-facing functions, classes, and more
#
def announce(path, mode, uid, gid):
    """Announce a service.
    """
    lisd = libruss.russ_announce(path, mode, uid, gid)
    return lisd

def dialv(deadline, op, spath, attrs=None, args=None):
    """Dial a service.
    """
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    cconn_ptr = libruss.russ_dialv(deadline, op, spath, c_attrs, c_argv)
    return bool(cconn_ptr) and ClientConn(cconn_ptr, True) or None

dial = dialv

def dialv_wait(deadline, op, spath, attrs=None, args=None):
    """Convenience function.
    """
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    exitst = ctypes.c_int()
    rv = libruss.russ_dialv_wait(deadline, op, spath, c_attrs, c_argv, ctypes.byref(exitst))
    return rv, int(exitst.value)

def dialv_wait_timeout(timeout, op, spath, attrs=None, args=None):
    return dialv_wait(to_deadline(timeout), op, spath, attrs, args)

def dialv_wait_inouterr(deadline, op, spath, attrs=None, args=None, stdin=None, stdout_size=STDOUT_SIZE_DEFAULT, stderr_size=STDERR_SIZE_DEFAULT):
    """Convenience function.
    """
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    exitst = ctypes.c_int()

    # create rbufs; exit on failure
    rbufs = [
        libruss.russ_buf_new(stdin and len(stdin) or 0),
        libruss.russ_buf_new(stdout_size),
        libruss.russ_buf_new(stderr_size),
    ]
    rbufs = [bool(rbuf) and rbuf or None for rbuf in rbufs]
    if None in rbufs:
        for i in xrange(3):
            libruss.russ_buf_free(rbufs[i])
        return RUSS_WAIT_FAILURE, RUSS_EXIT_FAILURE, None, None

    # copy stdin in
    if stdin:
        ctypes.memmove(rbufs[0].contents.data, ctypes.create_string_buffer(stdin), len(stdin))
        rbufs[0].contents.len = len(stdin)

    rv = libruss.russ_dialv_wait_inouterr3(deadline, op, spath, c_attrs, c_argv,
        ctypes.byref(exitst), rbufs[0], rbufs[1], rbufs[2])

    # copy stdout and stderr out
    stdout = ctypes.string_at(rbufs[1].contents.data, rbufs[1].contents.len)
    stderr = ctypes.string_at(rbufs[2].contents.data, rbufs[2].contents.len)

    # free rbufs
    for i in xrange(3):
        libruss.russ_buf_free(rbufs[i])

    return rv, int(exitst.value), stdout, stderr

def dialv_wait_inouterr_timeout(timeout, op, spath, attrs=None, args=None, stdin=None, stdout_size=STDOUT_SIZE_DEFAULT, stderr_size=STDERR_SIZE_DEFAULT):
    return dialv_wait_inouterr(to_deadline(timeout), op, spath, attrs, args, stdin, stdout_size, stderr_size)

def execv(deadline, spath, attrs=None, args=None):
    """ruexec a service.
    """
    return dialv(deadline, "execute", spath, attrs, args)

def execv_wait(deadline, spath, attrs=None, args=None):
    return dialv_wait(deadline, "execute", spath, attrs, args)

def execv_wait_timeout(timeout, spath, attrs=None, args=None):
    return dialv_wait_timeout(timeout, "execute", spath, attrs, args)

def execv_wait_inouterr(deadline, spath, attrs=None, args=None, stdin=None, stdout_size=STDOUT_SIZE_DEFAULT, stderr_size=STDERR_SIZE_DEFAULT):
    return dialv_wait_inouterr(deadline, "execute", spath, attrs, args, stdin, stdout_size, stderr_size)

def execv_wait_inouterr_timeout(timeout, op, spath, attrs=None, args=None, stdin=None, stdout_size=STDOUT_SIZE_DEFAULT, stderr_size=STDERR_SIZE_DEFAULT):
    return dialv_wait_inouterr_timeout(timeout, "execute", spath, attrs, args, stdin, stdout_size, stderr_size)

def gettime():
    """Get clock time (corresponds to a deadline).
    """
    return libruss.russ_gettime()

def switch_user(uid, gid, gids):
    """Change process uid/gid/supplemental gids.

    See also switch_userinitgroups().
    """
    if gids:
        _gids = (ctypes.c_int*len(gids))(gids)
    else:
        _gids = None
    return libruss.russ_switch_user(uid, gid, len(gids), _gids)

def switch_userinitgroups(uid, gid):
    """Change process uid/gid/supplemental gids; use initgroups().
    """
    return libruss.russ_switch_userinitgroups(uid, gid)

def to_deadline(timeout):
    """Convert timeout (ms) to deadline.
    """
    return libruss.russ_to_deadline(timeout)

def to_timeout(deadline):
    """Convert deadline to timeout (ms).
    """
    return libruss.russ_to_timeout(deadline)

def unlink(path):
    """Unlink service path.
    """
    return libruss.russ_unlink(path)

class Request:
    """Wrapper for russ_req object.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    def __del__(self):
        self._ptr = None

    def get_args(self):
        """Return a (copy) python list of args.
        """
        req_argv = self._ptr.contents.argv
        args = []
        if bool(req_argv):
            i = 0
            while 1:
                s = req_argv[i]
                i += 1
                if s == None:
                    break
                args.append(s)
        return  args

    def get_attrs(self):
        """Return a (copy) python dict of attrs.
        """
        req_attrv = self._ptr.contents.attrv
        attrs = {}
        if bool(req_attrv):
            i = 0
            while 1:
                s = req_attrv[i]
                i += 1
                if s == None:
                    break
                try:
                    k, v = s.split("=", 1)
                    attrs[k] = v
                except:
                    pass
        return attrs

    def get_op(self):
        """Return op string.
        """
        return self._ptr.contents.op
    op = property(get_op, None)

    def get_opnum(self):
        """Return op number.
        """
        return self._ptr.contents.opnum
    opnum = property(get_opnum, None)

    def get_protocolstring(self):
        """Return protocol string.
        """
        return self._ptr.contents.protocolstring
    protocolstring = property(get_protocolstring, None)

    def get_spath(self):
        """Return service path.
        """
        return self._ptr.contents.spath
    spath = property(get_spath, None)

class Conn:
    """Common (client, server) connection.

    If a connection is owned, then it will be freed by the
    destructor. This is typically appropriate only for ClientConn
    objects.
    """

    def __init__(self, _ptr, owned=False):
        self._ptr = _ptr
        self.owned = owned

    def get_fd(self, i):
        """Return fd by index.
        """
        if (i < 0) or (i >= RUSS_CONN_NFDS):
            raise Exception("bad fd index")
        return self._ptr.contents.fds[i]

    def get_fds(self):
        """Return list of fds.
        """
        return [self._ptr.contents.fds[i] for i in range(RUSS_CONN_NFDS)]

    def get_sd(self):
        """Return socket descriptor.
        """
        return self._ptr.contents.sd

    def get_sysfd(self, i):
        """Return sysfd by index.
        """
        if (i < 0) or (i >= RUSS_CONN_NSYSFDS):
            raise Exception("bad sysfd index")
        return self._ptr.contents.sysfds[i]

    def get_sysfds(self):
        """Return list of sysfds.
        """
        return [self._ptr.contents.sysfds[i] for i in range(RUSS_CONN_NSYSFDS)]

    def set_fd(self, i, value):
        """Set fd by index.
        """
        if (i < 0) or (i >= RUSS_CONN_NFDS):
            raise Exception("bad fd index")
        self._ptr.contents.fds[i] = value

    def set_sysfd(self, i, value):
        """Set sysfd by index.
        """
        if (i < 0) or (i >= RUSS_CONN_NSYSFDS):
            raise Exception("bad sysfd index")
        self._ptr.contents.sysfds[i] = value

class ClientConn(Conn):
    """Client connection.
    """

    def __del__(self):
        if self.owned:
            libruss.russ_cconn_free(self._ptr)
        self._ptr = None

    def close(self):
        """Close connection.
        """
        libruss.russ_cconn_close(self._ptr)

    def close_fd(self, i):
        """Close fd by index.
        """
        return libruss.russ_cconn_close_fd(self._ptr, i)

    def free(self):
        """Free underlying connection object.
        """
        libruss.russ_cconn_free(self._ptr)
        self._ptr = None

    def wait(self, deadline):
        """Wait for exit value.
        """
        exitst = ctypes.c_int()
        return libruss.russ_cconn_wait(self._ptr, deadline, ctypes.byref(exitst)), exitst.value

class Credentials:
    """Connection credentials.
    """

    def __init__(self, uid, gid, pid):
        self.uid = uid
        self.gid = gid
        self.pid = pid

    def __repr__(self):
        return "(uid=%d, gid=%d, pid=%s)" % (self.uid, self.gid, self.pid)

class ServerConn(Conn):
    """Server connection.
    """

    def __del__(self):
        if self.owned:
            libruss.russ_sconn_free(self._ptr)
        self._ptr = None

    def close(self):
        """Close connection.
        """
        libruss.russ_sconn_close(self._ptr)

    def close_fd(self, i):
        """Close fd by index.
        """
        return libruss.russ_sconn_close_fd(self._ptr, i)

    def free(self):
        """Free underlying connection object.
        """
        libruss.russ_sconn_free(self._ptr)
        self._ptr = None

    def get_creds(self):
        """Get connection credentials.
        """
        creds = self._ptr.contents.creds
        return Credentials(creds.uid, creds.gid, creds.pid)

    def answer(self, nfds, cfds):
        """Send fds to client as response.
        """
        if nfds:
            _cfds = (ctypes.c_int*nfds)(*tuple(cfds))
            rv = libruss.russ_sconn_answer(self._ptr, nfds,
                ctypes.cast(_cfds, ctypes.POINTER(ctypes.c_int)))
            # update python side (in-place)
            cfds[0:] = [fd for fd in _cfds]
            return rv
        else:
            return libruss.russ_sconn_answer(self._ptr, 0, None)

    def await_request(self, deadline):
        """Wait for incoming request.
        """
        return Request(libruss.russ_sconn_await_req(self._ptr, deadline))

    def exit(self, exitst):
        """Send exit value.
        """
        return libruss.russ_sconn_exit(self._ptr, exitst)

    def fatal(self, msg, exitst):
        """Send (usually non-0) exit value and message on stderr stream.
        """
        return libruss.russ_sconn_fatal(self._ptr, msg, exitst)

    def redialandsplice(self, deadline, cconn):
        """Dial another service and pass the received fds to the
        client.
        """
        return libruss.russ_sconn_redialandsplice(self._ptr, deadline, cconn._ptr)

    def splice(self, cconn):
        """Pass dialed connection fds to server client.
        """
        return libruss.russ_sconn_splice(self._ptr, cconn._ptr)

    def answerhandler(self):
        """Default answer handler.
        """
        return libruss.russ_sconn_answerhandler(self._ptr)
