#! /usr/bin/env python
#
# pyruss/server.py

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

"""Server and support for RUSS-based services.
"""

# system imports
try:
    from ConfigParser import ConfigParser as _ConfigParser
except:
    from configparser import ConfigParser as _ConfigParser
import os

#
import pyruss

class ConfigParser(_ConfigParser):
    """New ConfigParser which support an optional default value
    parameter like dict.get().
    """

    def get(self, section, option, default=None):
        try:
            return _ConfigParser.get(self, section, option)
        except:
            return default

    def getboolean(self, section, option, default=None):
        try:
            return _ConfigParser.getboolean(self, section, option)
        except:
            return default

    def getint(self, section, option, default=None):
        try:
            return _ConfigParser.getint(self, section, option)
        except:
            return default

    def getfloat(self, section, option, default=None):
        try:
            return _ConfigParser.getfloat(self, section, option)
        except:
            return default

class ServiceNode:
    """Used by ServiceTree in support of a hierarchy organized by
    path components. Each node contains ops, handler, type
    information.
    """

    def __init__(self, ops=None, handler=None, typ=None):
        self.ops = ops
        self.handler = handler
        self.typ = typ
        self.children = {}

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
        comps = path.split("/")
        for comp in comps[1:-1]:
            next_node = node.children.get(comp)
            if next_node == None:
                node = node.children[comp] = ServiceNode()
        node.children[comps[-1]] = ServiceNode(ops, handler, typ)

    def _find(self, comps):
        """Find node for path comps.
        """
        node = self.root
        for comp in comps:
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
        """
        req = conn.get_request()
        node = self.find(req.spath)
        if node and node.handler and (req.op in node.ops or req.op == None):
            node.handler(conn)
        else:
            self.fallback_handler(conn)
        conn.close()
        del conn

    def fallback_handler(self, conn):
        req = conn.get_request()
        op = req.op
        if op == "help":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "execute":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "info":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "list":
            node = self.find(req.spath)
            if node:
                if node.children:
                    os.write(conn.get_fd(1), "%s\n" % "\n".join(node.children))
            else:
                os.write(conn.get_fd(2), "error: no service available\n")
        else:
            os.write(conn.get_fd(2), "error: no service available\n")

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
        pyruss.close_listener_conn(self.lis)
        pyruss.free_listener_conn(self.lis)

    def announce(self, saddr, mode, uid, gid):
        """Announce service on filesystem.
        """
        self.saddr = saddr
        self.mode = mode
        self.uid = uid
        self.gid = gid
        self.lis = pyruss.announce(saddr, mode, uid, gid)

    def loop(self):
        if self.server_type == "fork":
            self.lis.loop(self.service_tree.handler)
