


loadfile '../site.lua'()

package.cpath = './Debug/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()

print( 'self=', Nylon.self )

