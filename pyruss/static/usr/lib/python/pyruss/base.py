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
    lis_ptr = libruss.russ_announce(path, mode, uid, gid)
    return bool(lis_ptr) and Listener(lis_ptr) or None

def dialv(deadline, op, spath, attrs, args):
    """Dial a service.
    """
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    cconn_ptr = libruss.russ_dialv(deadline, op, spath, c_attrs, c_argv)
    return bool(cconn_ptr) and ClientConn(cconn_ptr, True) or None

dial = dialv

def dialv_wait(deadline, op, spath, attrs, args):
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    exit_status = ctypes.c_int()
    rv = libruss.russ_dialv_wait(deadline, op, spath, c_attrs, c_argv, ctypes.byref(exit_status))
    return rv, int(exit_status.value)

def dialv_wait_inouterr(deadline, op, spath, attrs, args, stdin, stdout_size, stderr_size):
    c_attrs, c_argv = convert_dial_attrs_args(attrs, args)
    exit_status = ctypes.c_int()

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
        return -1, None, None, None

    # copy stdin in
    if stdin:
        ctypes.memmove(rbufs[0].contents.data, ctypes.create_string_buffer(stdin), len(stdin))
        rbufs[0].contents.len = len(stdin)

    rv = libruss.russ_dialv_wait_inouterr3(deadline, op, spath, c_attrs, c_argv,
        ctypes.byref(exit_status), rbufs[0], rbufs[1], rbufs[2])

    # copy stdout and stderr out
    stdout = ctypes.string_at(rbufs[1].contents.data, rbufs[1].contents.len)
    stderr = ctypes.string_at(rbufs[2].contents.data, rbufs[2].contents.len)

    # free rbufs
    for i in xrange(3):
        libruss.russ_buf_free(rbufs[i])

    return rv, int(exit_status.value), stdout, stderr

def execv(deadline, spath, attrs, args):
    """ruexec a service.
    """
    return dialv(deadline, "execute", spath, attrs, args)

def gettime():
    return libruss.russ_gettime()

def switch_user(uid, gid, gids):
    if gids:
        _gids = (ctypes.c_int*len(gids))(gids)
    else:
        _gids = None
    return libruss.russ_switch_user(uid, gid, len(gids), _gids)

def to_deadline(timeout):
    return libruss.russ_to_deadline(timeout)

def to_timeout(deadline):
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
        return self._ptr.contents.op
    op = property(get_op, None)

    def get_opnum(self):
        return self._ptr.contents.opnum
    opnum = property(get_opnum, None)

    def get_protocol_string(self):
        return self._ptr.contents.protocol_string
    protocol_string = property(get_protocol_string, None)

    def get_spath(self):
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
        return self._ptr.contents.fds[i]

    def get_fds(self):
        return [self._ptr.contents.fds[i] for i in range(RUSS_CONN_NFDS)]

    def get_sd(self):
        return self._ptr.contents.sd

    def set_fd(self, i, value):
        self._ptr.contents.fds[i] = value
        
class ClientConn(Conn):
    """Client connection.
    """

    def __del__(self):
        if self.owned:
            libruss.russ_cconn_free(self._ptr)            
        self._ptr = None

    def close(self):
        libruss.russ_cconn_close(self._ptr)

    def close_fd(self, i):
        return libruss.russ_cconn_close_fd(self._ptr, i)

    def free(self):
        libruss.russ_cconn_free(self._ptr)
        self._ptr = None

    def wait(self, deadline):
        exit_status = ctypes.c_int()
        return libruss.russ_cconn_wait(self._ptr, deadline, ctypes.byref(exit_status)), exit_status.value

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
        libruss.russ_sconn_close(self._ptr)

    def close_fd(self, i):
        return libruss.russ_sconn_close_fd(self._ptr, i)

    def free(self):
        libruss.russ_sconn_free(self._ptr)
        self._ptr = None

    def get_creds(self):
        creds = self._ptr.contents.creds
        return Credentials(creds.uid, creds.gid, creds.pid)

    def answer(self, nfds, cfds):
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
        return libruss.russ_sconn_await_request(self._ptr, deadline)

    def exit(self, exit_status):
        return libruss.russ_sconn_exit(self._ptr, exit_status)

    def fatal(self, msg, exit_status):
        return libruss.russ_sconn_fatal(self._ptr, msg, exit_status)

    def redial_and_splice(self, deadline, cconn):
        return libruss.russ_sconn_redial_and_splice(self._ptr, deadline, cconn._ptr)

    def splice(self, cconn):
        return libruss.russ_sconn_splice(self._ptr, cconn._ptr)

    def standard_answer_handler(self):
        return libruss.russ_standard_answer_handler(self._ptr)

class Listener:
    def __init__(self, _ptr):
        self._ptr = _ptr

    def __del__(self):
        #libruss.russ_lis_close(self._ptr)
        #libruss.russ_lis_free(self._ptr)
        self._ptr = None

    def accept(self, deadline):
        try:
            sconn_ptr = libruss.russ_lis_accept(self._ptr, deadline)
        except:
            traceback.print_exc()
        return bool(sconn_ptr) and ServerConn(sconn_ptr) or None

    def close(self):
        libruss.russ_lis_close(self._ptr)

    def get_sd(self):
        if self._ptr:
            return self._ptr.contents.sd
        else:
            return -1

    def loop(self, handler):
        """Fork-based loop.
        """
        while self.get_sd() >= 0:
            try:
                sconn = self.accept(RUSS_DEADLINE_NEVER)
                if sconn == None:
                    sys.stderr.write("error: cannot accept connection\n")
                    continue

                # double fork to satisfy waitpid() in parent (no zombies)
                pid = os.fork()
                if pid == 0:
                    os.setsid()
                    self.close()
                    if os.fork() == 0:
                        if sconn.await_request(RUSS_DEADLINE_NEVER) < 0:
                            sconn.close()
                            sys.exit(1)
                        try:
                            handler(sconn)
                        except:
                            pass
                        try:
                            if sconn:
                                sconn.fatal(RUSS_MSG_NO_EXIT, RUSS_EXIT_FAILURE)
                                sconn.close()
                        except:
                            pass
                    sys.exit(0)
                sconn.close()
                del sconn
                os.waitpid(pid, 0)
            except SystemExit:
                raise
            except:
                #traceback.print_exc()
                pass

    def loop_thread(self, handler):
        """Thread-based loop.
        """
        def pre_handler_thread(sconn, handler):
            if sconn.await_request(RUSS_DEADLINE_NEVER) < 0:
                return
            try:
                handler(sconn)
            except:
                pass
            try:
                if sconn:
                    sconn.fatal(RUSS_MSG_NO_EXIT, RUSS_EXIT_FAILURE)
                    sconn.close()
            except:
                pass

        while True:
            try:
                sconn = self.accept(RUSS_DEADLINE_NEVER)
                if sconn == None:
                    sys.stderr.write("error: cannot accept connection\n")
                    continue
                # no limiting of thread count
                Thread(target=pre_handler_thread, args=(sconn, req_handler)).start()
            except SystemExit:
                raise
            except:
                #traceback.print_exc()
                pass