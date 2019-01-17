#! /usr/bin/env python2
#
#  pyruss/__init__.py

"""pyruss is a Python implementation of RUSS, based on the russng C
implementation.

RUSS is an alternative to HTTP/web technologies for services running
on UNIX/Linux.

RUSS is a protocol and framework for building service-oriented
servers using UNIX/Domain sockets.

See https://expl.info/display/RUSS .
"""

from .bindings import *
from .conf import *
from .base import *
from .server import *
