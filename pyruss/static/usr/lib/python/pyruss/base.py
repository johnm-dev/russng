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
        
#
# Application-facing functions, classes, and more
#
def announce(path, mode, uid, gid):
    """Announce a service.
    """
    lis_ptr = libruss.russ_announce(path, mode, uid, gid)
    return bool(lis_ptr) and Listener(lis_ptr) or None

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
    return bool(conn_ptr) and ClientConn(conn_ptr) or None

dial = dialv

def execv(deadline, saddr, attrs, args):
    """ruexec a service.
    """
    return dialv(deadline, "execute", saddr, attrs, args)

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

class Conn:
    """Common (client, server) connection.
    """

    def __init__(self, _ptr):
        self._ptr = _ptr

    def __del__(self):
        #libruss.russ_conn_free(self._ptr)
        self._ptr = None

    def close(self):
        libruss.russ_conn_close(self._ptr)

    def close_fd(self, i):
        return libruss.russ_conn_close_fd(self._ptr, i)

    def get_creds(self):
        creds = self._ptr.contents.creds
        return Credentials(creds.uid, creds.gid, creds.pid)

    def get_fd(self, i):
        return self._ptr.contents.fds[i]

    def get_fds(self):
        return [self._ptr.contents.fds[i] for i in range(RUSS_CONN_NFDS)]

    def get_request(self):
        return self._ptr.contents.req

    def get_request_args(self):
        req = self._ptr.contents.req
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
        req = self._ptr.contents.req
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
        return self._ptr.contents.sd

    def set_fd(self, i, value):
        self._ptr.contents.fds[i] = value
        
    def splice(self, dconn):
        return libruss.russ_conn_splice(self._ptr, dconn._ptr)

class ClientConn(Conn):
    """Client connection.
    """

    def wait(self, deadline):
        exit_status = ctypes.c_int()
        return libruss.russ_conn_wait(self._ptr, ctypes.byref(exit_status), deadline), exit_status.value

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

    def answer(self, nfds, cfds, sfds):
        if 0:
            # TODO: how to handle passing fds?
            return libruss.russ_conn_answer(self._ptr, nfds, ctypes.POINTER(cfds), ctypes.POINTER(sfds))
        else:
            return libruss.russ_conn_answer(self._ptr, 0, None, None)

    def await_request(self, deadline):
        return libruss.russ_conn_await_request(self._ptr, deadline)

    def exit(self, exit_status):
        return libruss.russ_conn_exit(self._ptr, exit_status)

    def exits(self, msg, exit_status):
        return libruss.russ_conn_exits(self._ptr, msg, exit_status)

    def fatal(self, msg, exit_status):
        return libruss.russ_conn_fatal(self._ptr, msg, exit_status)

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
            conn_ptr = libruss.russ_lis_accept(self._ptr, deadline)
        except:
            traceback.print_exc()
        return bool(conn_ptr) and ServerConn(conn_ptr) or None

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
                conn = self.accept(RUSS_DEADLINE_NEVER)
                if conn == None:
                    sys.stderr.write("error: cannot accept connection\n")
                    continue

                # double fork to satisfy waitpid() in parent (no zombies)
                pid = os.fork()
                if pid == 0:
                    os.setsid()
                    self.close()
                    if os.fork() == 0:
                        if conn.await_request(RUSS_DEADLINE_NEVER) < 0:
                            conn.close()
                            sys.exit(1)
                        try:
                            handler(conn)
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
                os.waitpid(pid, 0)
            except SystemExit:
                raise
            except:
                #traceback.print_exc()
                pass

    def loop_thread(self, handler):
        """Thread-based loop.
        """
        def pre_handler_thread(conn, handler):
            if conn.await_request(RUSS_DEADLINE_NEVER) < 0:
                return
            try:
                handler(conn)
            except:
                pass
            try:
                if conn:
                    conn.fatal(RUSS_MSG_NO_EXIT, RUSS_EXIT_FAILURE)
                    conn.close()
            except:
                pass

        while True:
            try:
                conn = self.accept(RUSS_DEADLINE_NEVER)
                if conn == None:
                    sys.stderr.write("error: cannot accept connection\n")
                    continue
                # no limiting of thread count
                Thread(target=pre_handler_thread, args=(conn, req_handler)).start()
            except SystemExit:
                raise
            except:
                #traceback.print_exc()
                pass