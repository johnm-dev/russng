#! /usr/bin/env ruby1.9.1
#
# rbruss/bindings.rb

# license--start
#
#  This file is part of the rbruss library.
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

"""Ruby bindings to the russ library C API.
"""

require "ffi"

module RussLib
  extend FFI::Library
  ffi_lib "libruss.so"

  # russ_dialv
  attach_function 'russ_dialv', [ :string, :string, :int, :pointer, :int, :pointer ], :pointer
  attach_function 'russ_close_conn', [ :pointer ], :void
  attach_function 'russ_close_fd', [ :pointer, :int ], :void
  attach_function 'russ_free_conn', [ :pointer ], :void
  attach_function 'russ_loop', [ :pointer, :pointer ], :void

  class Russ_Credentials_Structure < FFI::Struct
    layout :pid, :long,
      :uid, :long,
      :gid, :long
  end

  class Russ_Request_Structure < FFI::Struct
    layout :protocol_string, :string,
      :spath, :string,
      :op, :string,
      :attrc, :int,
      :attrv, :pointer,
      :argc, :int,
      :argv, :pointer
  end

  class Russ_Conn_Structure < FFI::Struct
    layout :conn_type, :int,
      :cred, Russ_Credentials_Structure,
      :req, Russ_Request_Structure,
      :sd, :int,
      :fds, [ :int, 3 ]
  end

  # Dial a service
  def self.dial(saddr, op, timeout, attrs, args)
    if attrs.nil? or attrs.length == 0
      p = "AA.1"
    else
      c_attrs = FFI::MemoryPointer.new(:pointer, attrs.length+1)
      (0..attrs.length).to_a.zip(attrs.keys) {|i, k|
        c_attrs[i] = "${k}=${attrs[k]}"
      }
      c_attrs[attrs.length] = nil
    end

    # can this use *args?
    if args.nil? or attrs.length == 0
      args = nil
      args_length = 0
    else
      c_argv = FFI::MemoryPointer.new(:pointer, args.length+1)
      (0..args.length).to_a.zip(args) {|i, v|
        c_argv[i] = args[i]
      }
      args_length = args.length
    end
    return ClientConn.new(RussLib.russ_dialv(saddr, op, timeout, c_attrs, args_length, c_argv))
  end

  # Common (client, server) connection.
  class Conn
    @raw_conn
    @ptr_conn

    def initialize(raw_conn)
      @raw_conn = raw_conn
      #@ptr_conn = ctypes.cast(raw_conn, ctypes.POINTER(russ_conn_Structure))
      @ptr_conn = Russ_Conn_Structure.new(raw_conn)
    end

    def release
      RussLib.russ_free_conn(@raw_conn)
      @raw_conn = nil
      @ptr_conn = nil
    end

    def close_fd(i)
      return RussLib.russ_close_fds(i, @raw_conn.fds)
    end

    #def get_cred(self):
        #cred = self.ptr_conn.cred
        #return (cred.pid, cred.uid, cred.gid)

    def get_fd(i)
      #return @ptr_conn.fds[i]
      return @ptr_conn[:fds][i]
    end

    def get_fd2(i)
      p @raw_conn.inspect
      p @ptr_conn.inspect
      fds = @ptr_conn[:fds].read_array_of_int(3)
    end

    def debug
      p "conn_type #{@ptr_conn[:conn_type]}"
      p "cred #{@ptr_conn[:cred]}"
      p "req #{@ptr_conn[:req]}"
      (0..2).to_a.each {|i| p "fds[#{i}] #{@ptr_conn[:fds][i]}" }
      p "sd #{@ptr_conn[:sd]}"
      nil
    end

    #def get_request(self):
        #return self.ptr_conn.contents.req

    #def get_request_args(self):
        #req = self.ptr_conn.contents.req
        #return [req.argv[i] for i in range(req.argc)]

    #def get_request_attrs(self):
        #req = self.ptr_conn.contents.req
        #attrs = {}
        #for i in xrange(req.attrc):
            #s = req.attrv[i]
            #try:
                #k, v = s.split("=", 1)
                #attrs[k] = v
            #except:
                #pass
        #return attrs

    def get_sd
      return @ptr_conn[:sd]
    end

    def close
      RussLib.russ_close_conn(@raw_conn)
    end
  end

  # Client connection.
  class ClientConn < Conn
  end
end
