#! /usr/bin/env python
#
# pyruss/conf.py

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
    from ConfigParser import ConfigParser
except:
    from configparser import ConfigParser
import os
import sys

class Conf(ConfigParser):
    """New ConfigParser which support an optional default value
    parameter like dict.get().
    """

    def __init__(self, argv, print_usage):
        Conf.__init__(self)
        args = argv[1:]
        while 1:
            arg = args.pop(0)
            if arg == "-c" and args:
                try:
                    section, rest = args.pop(0).split(":", 1)
                    option, value = rest.split("=", 1)
                    self.set2(section, option, value)
                except:
                    raise Exception()
            elif arg == "-f" and args:
                try:
                    self.read(args.pop(1))
                except:
                    raise Exception()
            elif arg == "-h":
                print_usage()
                os.exit(0)
            elif arg == "--":
                break
        del argv[1:]
        argv.extend(args)

    def get(self, section, option, default=None):
        try:
            return ConfigParser.get(self, section, option)
        except:
            return default

    def getboolean(self, section, option, default=None):
        try:
            return ConfigParser.getboolean(self, section, option)
        except:
            return default

    def getint(self, section, option, default=None):
        try:
            return ConfigParser.getint(self, section, option)
        except:
            return default

    def getfloat(self, section, option, default=None):
        try:
            return ConfigParser.getfloat(self, section, option)
        except:
            return default

    def set2(self, section, option, value):
        if not self.has_section(section):
            self.add_section(section)
        self.set(section, option, value)
