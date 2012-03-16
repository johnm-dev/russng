#! /usr/bin/env python
#
# papyruss/server.py

"""Server and support for RUSS-based services.
"""

# system imports
import os

#
import papyruss

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
        op_handler = self.op_handlers.get(conn.spath)
        if op_handler:
            op_handler(conn)
        else:
            op_error(conn)
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
        handler = self.svc_handlers.get(conn.spath)
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
