#! /usr/bin/env python3
#! /usr/bin/env python2
#
# pyruss/conf.py

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
    from ConfigParser import RawConfigParser
except:
    from configparser import RawConfigParser
import os
import sys
import traceback

class Conf(RawConfigParser):
    """New RawConfigParser (no interpolation) which supports an
    optional default value parameter like dict.get().
    """

    def __init__(self):
        RawConfigParser.__init__(self)

    def init(self, args):
        """Given a list of conf option flags and values, update
        the Conf object and return a new list of args pruned of
        those used. "--" stops processing (and is not returned in
        args list).
        """
        self.optionxform = lambda option: option
        args = args[:]
        while args:
            arg = args.pop(0)
            if arg == "-c" and args:
                try:
                    section, rest = args.pop(0).split(":", 1)
                    option, value = rest.split("=", 1)
                    self.set2(section, option, value)
                except:
                    raise Exception()
            elif arg == "-d" and args:
                try:
                    section = args.pop(0)
                    if ":" in section:
                        section, option = section.split(":", 1)
                        self.remove_option(section, option)
                    else:
                        self.remove_section(section)
                except:
                    pass
            elif arg == "-f" and args:
                try:
                    self.read(args.pop(0))
                except:
                    raise Exception()
            elif arg == "--fd" and args:
                try:
                    self.readfp(os.fdopen(int(args.pop(0))))
                except:
                    raise Exception()
            elif arg == "--":
                break

        # (RUSSNG-899) copy "server" to "main" (remove in v7)
        if not self.has_section("main") and self.has_section("server"):
            self.add_section("main")
            for name, value in self.items("server"):
                self.set("main", name, value)

        return args

    def get(self, section, option, default=None, **kwargs):
        try:
            return RawConfigParser.get(self, section, option)
        except:
            return default

    def getboolean(self, section, option, default=None):
        try:
            return RawConfigParser.getboolean(self, section, option)
        except:
            return default

    def getint(self, section, option, default=None):
        try:
            return RawConfigParser.getint(self, section, option)
        except:
            return default

    def getfloat(self, section, option, default=None):
        try:
            return RawConfigParser.getfloat(self, section, option)
        except:
            return default

    def set2(self, section, option, value):
        if not self.has_section(section):
            self.add_section(section)
        self.set(section, option, value)
