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

class ServiceNode:
    """Used by ServiceTree in support of a hierarchy organized by
    path components. Each node contains ops, handler, type
    information.
    """

    def __init__(self, ops=None, handler=None, typ=None):
        self.set(ops, handler, typ)
        self.children = {}

    def set(self, ops, handler, typ):
        self.ops = ops
        self.handler = handler
        self.typ = typ

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

    def add(self, path, ops, handler, typ=None):
        """Add service node to tree.
        """
        node = self.root
        if path == "/":
            node.set(ops, handler, typ)
        else:
            comps = path.split("/")
            for comp in comps[1:-1]:
                next_node = node.children.get(comp)
                if next_node == None:
                    next_node = node.children[comp] = ServiceNode()
                node = next_node
            node.children[comps[-1]] = ServiceNode(ops, handler, typ)

    def _find(self, comps):
        """Find node for path comps.
        """
        node = self.root
        for comp in comps:
            if comp == "":
                # don't change current node
                continue
            node = node.children.get(comp)
            if node == None:
                break
        return node

    def find(self, path):
        """Find node for path.
        """
        node = self.root
        return self._find(path.split("/")[1:])

    def find_children(self, path):
        """Find node for path and return node's children.
        """
        node = self._find(path.split("/")[1:])
        if node:
            return node.children
        else:
            return None

    def remove(self, path):
        """Remove node from tree: handler if node has children,
        otherwise whole node.
        """
        comps = path.split("/")
        parent_node = self._find(comps[1:-1])
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

    def handler(self, conn):
        """Select op handler and call.
        
        Special cases:
        * op == "help" - fallback to spath == "/" if available
        * op == "list" - list node.children if found
        
        Calling conn.exit() is left to the service handler. All
        connection fds are closed before returning.
        conn.exit(pyruss.RUSS_EXIT_FAILURE) is a fallback.
        """
        req = conn.get_request()
        node = self.find(req.spath)
        if node and node.handler and (req.op in node.ops or req.op == None):
            # handler registered for op and spath
            node.handler(conn)
        elif op == "list":
            # default handling for "list"; list "children" at spath
            node = self.find(req.spath)
            if node:
                if node.children:
                    os.write(conn.get_fd(1), "%s\n" % "\n".join(sorted(node.children)))
                conn.exit(0)
            else:
                conn.fatal(pyruss.RUSS_MSG_NO_SERVICE, pyruss.RUSS_EXIT_FAILURE)
        elif op == "help":
            # default handling for "help"; use node spath == "/"
            node = self.find("/")
            if node:
                node.handler(conn)
                conn.exit(0)
        else:
            conn.fatal(pyruss.RUSS_MSG_NO_SERVICE, pyruss.RUSS_EXIT_FAILURE)

        # clean up
        conn.exit(pyruss.RUSS_EXIT_FAILURE)
        conn.close()
        del conn

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

    def loop(self, answer_handler=None, accept_handler=None, request_handler=None):
        request_handler = request_handler or self.service_tree.handler

        if self.server_type == "fork":
            self.lis.loop(answer_handler, accept_handler, self.service_tree.handler)
        elif self.server_type == "thread":
            self.lis.loop_thread(answer_handler, accept_handler, self.service_tree.handler)
