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

module Russ
  extend FFI::Library
  ffi_lib "libruss.so"

  #
  # C API structures
  #

  class Russ_Listener_Structure < FFI::Struct
    layout :sd, :int
  end

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

  #
  # C API signatures
  #

  callback :handler_func, [:int, :pointer], :int

  attach_function 'russ_dialv', [:string, :string, :int, :pointer, :int, :pointer], :pointer
  attach_function 'russ_close_conn', [:pointer], :void
  attach_function 'russ_close_fd', [:pointer, :int], :void
  attach_function 'russ_free_conn', [:pointer], :void
  attach_function 'russ_loop', [:pointer, :pointer], :void
  attach_function 'russ_answer', [:pointer, :int], :pointer
  attach_function 'russ_accept', [:pointer, :pointer, :pointer], :int
  attach_function 'russ_await_request', [:pointer], :int
  attach_function 'russ_close_listener', [:pointer], :void
  attach_function 'russ_free_listener', [:pointer], :pointer
  attach_function 'russ_loop', [:pointer, :handler_func], :void

  # not supporting russ_exec*()
  attach_function 'russ_help', [:string, :int], :pointer
  attach_function 'russ_info', [:string, :int], :pointer
  attach_function 'russ_list', [:string, :int], :pointer

  #
  # ruby API
  #

  def self.exec(saddr, timeout, attrs, args)
    return self.dial(saddr, "execute", timeout, attrs, args)
  end

  def self.help(saddr, timeout)
    return ClientConn.new(Russ.russ_help(saddr, timeout))
  end

  def self.info(saddr, timeout)
    return ClientConn.new(Russ.russ_info(saddr, timeout))
  end

  def self.list(saddr, timeout)
    return ClientConn.new(Russ.russ_list(saddr, timeout))
  end

  # dial a service
  def self.dial(saddr, op, timeout, attrs, args)
    if attrs.nil? or attrs.length == 0
      c_attrs = nil
    else
      c_attrs = FFI::MemoryPointer.new(:pointer, attrs.length+1)
      (0..attrs.length).zip(attrs.keys) {|i, k|
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
      #(0..args.length).to_a.zip(args) {|i, v|
      args.each_with_index {|v, i|
        c_argv[i] = args[i]
      }
      args_length = args.length
    end
    return ClientConn.new(Russ.russ_dialv(saddr, op, timeout, c_attrs, args_length, c_argv))
  end

  # announce service
  def announce(path, mode, uid, gid)
    return Listener.new(Russ.russ_announce(path, mode, uid, gid))
  end

  # common (client, server) connection
  class Conn
    @raw_conn
    @ptr_conn

    def initialize(raw_conn)
      @raw_conn = raw_conn
      #@ptr_conn = ctypes.cast(raw_conn, ctypes.POINTER(russ_conn_Structure))
      @ptr_conn = Russ_Conn_Structure.new(raw_conn)
    end

    def release
      Russ.russ_free_conn(@raw_conn)
      @raw_conn = nil
      @ptr_conn = nil
    end

    def close_fd(i)
      return Russ.russ_close_fds(i, @raw_conn.fds)
    end

    def get_cred
        cred = @ptr_conn[:cred]
        return [cred.pid, cred.uid, cred.gid]
    end

    def get_fd(i)
      return @ptr_conn[:fds][i]
    end

    def debug
      p "conn_type #{@ptr_conn[:conn_type]}"
      p "cred #{@ptr_conn[:cred]}"
      p "req #{@ptr_conn[:req]}"
      (0..2).each {|i| p "fds[#{i}] #{@ptr_conn[:fds][i]}" }
      p "sd #{@ptr_conn[:sd]}"
      nil
    end

    def get_request
      return @ptr_conn[:req]
    end

    def get_request_args
      req = @ptr_conn[:req]
      args = []
      (0..req[:argc]).each {|i|
        args.push(req[:argv][i])
      }
    end

    def get_request_attrs
      req = @ptr_conn[:req]
      attrs = {}
      (0..req[:attrc]).each {|i|
        s = req[:attrv][i]
        begin
          k, v = s.split("=", 2)
          attrs[k] = v
        rescue => detail
        end
      }
      return attrs
    end

    def get_sd
      return @ptr_conn[:sd]
    end

    def close
      Russ.russ_close_conn(@raw_conn)
    end
  end

  # client connection
  class ClientConn < Conn
  end

  # server connection
  class ServerConn < Conn
    def accept(cfds, sfds)
      _cfds = FFI::MemoryPointer(:int, 3)
      _sfds = FFI::MemoryPointer(:int, 3)
      _cfds.write_array_of_int(cfds)
      _sfds.write_array_of_int(sfds)
      Russ.russ_accept(@raw_conn, _cfds, _sfds)
    end

    def await_request
      Russ.russ_await_request(@raw_conn)
    end
  end

  class Listener
    @raw_lis

    def initialize(raw_lis)
      @raw_lis = raw_lis
    end

    def release
      Russ.russ_free_listener(@raw_lis)
      @raw_lis = nil
    end

    def answer(timeout)
      return ServerConn(Russ.russ_answer(@raw_lis, timeout))
    end

    def close
      Russ.russ_close_listener(@raw_lis)
    end

    def loop(handler)
      def raw_handler(raw_conn)
        handler(ServerConn(raw_conn))
        return 0    # TODO: allow a integer return value from handler
      end
      Russ.russ_loop(@raw_lis, raw_handler)
    end
  end
end
