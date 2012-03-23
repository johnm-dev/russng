#! /usr/bin/env python
#
# papyruss/server.py

"""Server and support for RUSS-based services.
"""

# system imports
try:
    from ConfigParser import ConfigParser as _ConfigParser
except:
    from configparser import ConfigParser as _ConfigParser
import os

#
import papyruss

class ConfigParser(_ConfigParser):

    def get(self, section, option, default=None):
        try:
            return _ConfigParser.get(self, section, option)
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
    def __init__(self, handler=None, typ=None):
        self.handler = handler
        self.typ = typ
        self.children = {}

class ServiceTree2:

    def __init__(self):
        self.root = ServiceNode()

    def add(self, path, handler):
        """Add service node to tree.
        """
        node = self.root
        comps = path.split("/")
        for comp in comps[1:-1]:
            next_node = node.children.get(comp)
            if next_node == None:
                node = node.children[comp] = ServiceNode()
        node.children[comps[-1]] = ServiceNode(handler)

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
        svc_handler = self.find(req.spath)
        if svc_handler:
            svc_handler(conn)
        else:
            self.fallback_handler(conn)
        conn.close()
        del conn

    def fallback_handler(self, conn):
        op = conn.op
        req = conn.get_request()
        if op == "help":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "execute":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "info":
            os.write(conn.get_fd(2), "error: no service available\n")
        elif op == "list":
            os.write(conn.get_fd(2), "error: no service available\n")
        else:
            os.write(conn.get_fd(2), "error: no service available\n")

class ServiceTree:
    """Manage hierarchical (or not) service names and handlers to be
    called based on the connection request.
    """

    def __init__(self):
        self.svc_handlers = {}
        self.op_handlers = {
            "help": self.op_help,
            "info": self.op_info,
            "execute": self.op_execute,
            "list": self.op_list,
        }

    def add_op_handler(self, name, handler):
        self.op_handlers[name] = handler

    def add_service(self, name, handler):
        self.svc_handlers[name] = handler

    def handler(self, conn):
        """Select op handler and call.
        """
        req = conn.get_request()
        op_handler = self.op_handlers.get(req.op)
        if op_handler:
            op_handler(conn)
        else:
            self.op_error(conn)
        conn.close()
        del conn

    def op_error(self, conn):
        """Fallback operation; send error message.
        """
        os.write(conn.get_fd(2), "error: operation not supported\n")

    def op_help(self, conn):
        """Send help/usage information.
        """
        os.write(conn.get_fd(2), "error: help not available\n")

    def op_info(self, conn):
        """Send optional server information.
        """
        os.write(conn.get_fd(2), "error: server info not available\n")

    def op_execute(self, conn):
        """Execute the service.
        """
        req = conn.get_request()
        handler = self.svc_handlers.get(req.spath)
        if handler:
            handler(conn)

    def op_list(self, conn):
        """Send a list of service names.
        """
        keys = sorted(self.handlers.keys())
        keys.append("")
        os.write(conn.get_fd(1), "\n".join(keys))

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
        papyruss.close_listener_conn(self.lis)
        papyruss.free_listener_conn(self.lis)

    def announce(self, saddr, mode, uid, gid):
        """Announce service on filesystem.
        """
        self.saddr = saddr
        self.mode = mode
        self.uid = uid
        self.gid = gid
        self.lis = papyruss.announce(saddr, mode, uid, gid)

    def loop(self):
        if self.server_type == "fork":
            self.lis.loop(self.service_tree.handler)
