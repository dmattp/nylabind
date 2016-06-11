loadfile '../site.lua'()

package.cpath = './Debug/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()
require 'zmqnylon'

local ctx = Zmq.Context(1)

print('ctx=',ctx)

print('req=',Zmq.Socket.req)

local s = Zmq.Socket( ctx, Zmq.Socket.req )

-- os.exit(0)

local function tfunc(cord)

   print('s=',s)

   print "(0) Connecting to hello world server..."

   s:connect 'tcp://localhost:5555'

   print "(1) Connected to server, sending hello..."

   cord:cthreaded( s:ctsend 'Hello' )

   print "(2) sent hello..."

   local resp = cord:cthreaded( s:ctrecv() )

   print( "(3) got response=", resp )

   cord:sleep(1)

   print( "(4) exiting", resp )

   os.exit(0)
end

Nylon.cord( 'x', tfunc )


Nylon.run()

        
