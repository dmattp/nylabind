loadfile '../site.lua'()

package.cpath = './Debug/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()
require 'zmqnylon'

local ctx = Zmq.Context(1)

print('ctx=',ctx)

print('req=',Zmq.Socket.req)

local s = Zmq.Socket( ctx, Zmq.Socket.rep )

s:bind 'tcp://*:5555'

local function tfunc(cord)

   print('s=',s)

   while true do
      print "(0) waiting for incoming 'hello' message..."

      local incoming = cord:cthreaded( s:ctrecv() )

      print( "(1) Got incoming hello, msg=", incoming )

      cord:cthreaded( s:ctsend 'World' )

      print "(2) sent World response..."
   end
end

Nylon.cord( 'x', tfunc )


Nylon.run()

        
