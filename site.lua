
if string.find(arg[-1],'lua523r') then
   package.cpath = package.cpath .. ';../../nylon/Release/?.dll;../../nylabind/bin/Release/?.dll' -- package.path = package.path .. ';../../nylon/Release/?.lua'
else
   package.cpath = package.cpath .. ';../../nylon/Debug/?.dll;../../nylabind/bin/Debug/?.dll'
end

package.path  = package.path .. ';../../nylon/?.lua;../site/?.lua;../?.lua'

