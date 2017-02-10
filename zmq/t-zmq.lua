loadfile '../site.lua'()

package.cpath = './Debug/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()
require 'zmqnylon'


Nylon.cord( 'x', function(cord)
   local ctx = Zmq.Context(1)

   print('ctx=',ctx)
   
   print('req=',Zmq.Socket.req)
   
   local s = Zmq.Socket( ctx, Zmq.Socket.req )
   
   print('s=',s)
   
   print "Connecting to hello world server..."
   
   s:connect 'tcp://localhost:7755'
   
   print "Connected to server, sending hello..."
   
   s:send '{"format":3, "id":12345, "module":"ZMQModuleTest", "level":1, "msg":"hello from ZMQ!"}'

   print "wait reply"
   local r = s:recv()
   assert( r == '0' )

   cord:sleep(0.5)

   for i=1,3 do
      s:send( '{"format":3, "id":' .. tostring(i) .. ', "module":"ZMQModuleTest", "level":1, "msg":"hello from ZMQ!"}')
      local r = s:recv()
      assert( r == '0' )
   end
   
   
--   local resp = s:recv()
--   print( "got response=", resp )
end)


Nylon.run()

        
