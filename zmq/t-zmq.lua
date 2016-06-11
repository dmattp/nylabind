loadfile '../site.lua'()

package.cpath = './Debug/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()
require 'zmqnylon'

local ctx = Zmq.Context(1)

print('ctx=',ctx)

print('req=',Zmq.Socket.req)

local s = Zmq.Socket( ctx, Zmq.Socket.req )

print('s=',s)

print "Connecting to hello world server..."

s:connect 'tcp://localhost:5555'

print "Connected to server, sending hello..."

s:send 'Hello'

local resp = s:recv()

print( "got response=", resp )

-- os.exit(0)

Nylon.cord( 'x', function(cord)
               --while true do
                  cord:sleep(1)
--               end
end)


Nylon.run()

        
