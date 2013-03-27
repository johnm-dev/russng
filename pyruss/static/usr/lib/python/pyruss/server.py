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
import pyruss

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

    def get_fds(self):
        return [self.conn_ptr.contents.fds[i] for i in range(RUSS_CONN_NFDS)]

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

    def set_fd(self, i, value):
        self.conn_ptr.contents.fds[i] = value
        
    def splice(self, dconn):
        return libruss.russ_conn_splice(self.conn_ptr, dconn.conn_ptr)

class ClientConn(Conn):
    """Client connection.
    """

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

    def answer(self, nfds, cfds, sfds):
        if 0:
            # TODO: how to handle passing fds?
            return libruss.russ_conn_answer(self.conn_ptr, nfds, ctypes.POINTER(cfds), ctypes.POINTER(sfds))
        else:
            return libruss.russ_conn_answer(self.conn_ptr, 0, None, None)

    def await_request(self, deadline):
        return libruss.russ_conn_await_request(self.conn_ptr, deadline)

    def exit(self, exit_status):
        return libruss.russ_conn_exit(self.conn_ptr, exit_status)

    def exits(self, msg, exit_status):
        return libruss.russ_conn_exits(self.conn_ptr, msg, exit_status)

    def fatal(self, msg, exit_status):
        return libruss.russ_conn_fatal(self.conn_ptr, msg, exit_status)

    def standard_answer_handler(self):
        return libruss.russ_standard_answer_handler(self.conn_ptr)

class Listener:
    def __init__(self, lis_ptr):
        self.lis_ptr = lis_ptr

    def __del__(self):
        libruss.russ_lis_close(self.lis_ptr)
        libruss.russ_lis_free(self.lis_ptr)
        self.lis_ptr = None

    def accept(self, deadline):
        try:
            conn_ptr = libruss.russ_lis_accept(self.lis_ptr, deadline)
        except:
            traceback.print_exc()
        return bool(conn_ptr) and ServerConn(conn_ptr) or None

    def close(self):
        libruss.russ_lis_close(self.lis_ptr)

    def get_sd(self):
        if self.lis_ptr:
            return self.lis_ptr.contents.sd
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

class ServiceNode:
    """Used by ServiceTree in support of a hierarchy organized by
    path components. Each node contains handler, type
    information.
    """

    def __init__(self, handler=None, typ=None):
        self.set(handler, typ)
        self.children = {}

    def set(self, handler, typ):
        self.handler = handler
        self.typ = typ

class ServiceContext:
    """Provides context to service handler.
    """

    def __init__(self):
        self.spath = ""

class ServiceTree:
    """Provides a hierarchy of ServiceNode objects. Nodes in the
    hierarchy can be added, removed, and searched for based on a
    path. The ServiceTree provides a handler method matched by path
    and op; no match falls back to the fallback_handler method. Each
    of the handler methods may be overridden. The default
    fallback_handler implementation supports the "list" operation
    which returns a list of children for the path.
    """

    def __init__(self):
        self.root = ServiceNode()

    def add(self, path, handler, typ=None):
        """Add service node to tree.
        """
        node = self.root
        if path == "/":
            node.set(handler, typ)
        else:
            comps = path.split("/")
            for comp in comps[1:-1]:
                next_node = node.children.get(comp)
                if next_node == None:
                    next_node = node.children[comp] = ServiceNode()
                node = next_node
            node.children[comps[-1]] = ServiceNode(handler, typ)

    def _find(self, comps):
        """Find node for path comps and return the comps for each
        matched node starting at the root node. Since empty ("")
        path components are ignored, path.split("/") can be passed
        directly.
        """
        node = self.root
        ncomps = []
        for comp in comps:
            if comp == "":
                # don't change current node
                continue
            node2 = node.children.get(comp)
            if node2 == None:
                break
            node = node2
            ncomps.append(comp)
        return node, ncomps

    def find(self, path):
        """Find node for path and return matched path.
        """
        node = self.root
        node, ncomps = self._find(path.split("/")[1:])
        if ncomps:
            npath = "/"+"/".join(ncomps)
        else:
            npath = ""
        return node, npath

    def find_children(self, path):
        """Find node for path and return node's children.
        """
        node, _ = self._find(path.split("/")[1:])
        if node:
            return node.children
        else:
            return None

    def remove(self, path):
        """Remove node from tree: handler if node has children,
        otherwise whole node.
        """
        comps = path.split("/")
        parent_node, _ = self._find(comps[1:-1])
        try:
            node = parent_node.children.get(comps[-1])
            if len(node.children) == 0:
                del parent_node[comps[-1]]
            else:
                node.handler = None
                node.typ = None
        except:
            pass

    def remove_all(self, path):
        """Remove node and children from tree.
        """
        comps = path.split("/")
        parent_node = self._find(comps[1:-1])
        try:
            del parent_node[comps[-1]]
        except:
            pass

class Server:
    """Server to handle requests and service them.
    """

    def __init__(self, service_tree, server_type):
        self.service_tree = service_tree
        self.server_type = server_type

        self.saddr = None
        self.mode = None
        self.uid = None
        self.gid = None
        self.lis = None

    def __del__(self):
        self.lis.close()

    def announce(self, saddr, mode, uid, gid):
        """Announce service on filesystem.
        """
        self.saddr = saddr
        self.mode = mode
        self.uid = uid
        self.gid = gid
        self.lis = pyruss.announce(saddr, mode, uid, gid)

    def handler(self, conn):
        """Find service handler and invoke it.
        
        Special cases:
        * opnum == RUSS_OPNUM_HELP - fallback to spath == "/" if available
        * opnum == RUSS_OPNUM_LIST - list node.children if found
        
        Calling conn.exit() is left to the service handler. All
        connection fds are closed before returning.
        conn.exit(pyruss.RUSS_EXIT_FAILURE) is a fallback.

        TODO: when req.spath is not found within the service tree,
            node == None and no service is found. this needs to be
            fixed so that a (leaf) node could service the request
            based on a partial match of req.spath
        """
        req = conn.get_request()
        ctxt = ServiceContext()
        node, ctxt.spath = self.service_tree.find(req.spath)

        # call standard_answer_handler() for now
        conn.standard_answer_handler()

        if req.opnum == pyruss.RUSS_OPNUM_LIST:
            # default handling for "list"; list "children" at spath
            if req.spath in ["/", ctxt.spath]:
                os.write(conn.get_fd(1), "%s\n" % "\n".join(sorted(node.children)))
                conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            else:
                conn.fatal(pyruss.RUSS_MSG_NO_SERVICE, pyruss.RUSS_EXIT_FAILURE)
        elif req.opnum == pyruss.RUSS_OPNUM_HELP:
            # default handling for "help"; use node spath == "/"
            node, ctxt.spath = self.service_tree.find("/")
            if req.spath in ["/", ctxt.spath]:
                node.handler(conn, ctxt)
                conn.exit(pyruss.RUSS_EXIT_SUCCESS)
            else:
                conn.fatal(pyruss.RUSS_MSG_NO_SERVICE, pyruss.RUSS_EXIT_FAILURE)
        elif node.handler:
            # service request from this tree for all other ops
            node.handler(conn, ctxt)
        else:
            conn.fatal(pyruss.RUSS_MSG_NO_SERVICE, pyruss.RUSS_EXIT_FAILURE)

        # clean up
        conn.exit(pyruss.RUSS_EXIT_FAILURE)
        conn.close()
        del conn

    def loop(self):
        if self.server_type == "fork":
            self.lis.loop(self.handler)
        elif self.server_type == "thread":
            self.lis.loop_thread(self.handler)
