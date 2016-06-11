loadfile '../site.lua'()

package.cpath = './Release/?.dll;' .. package.cpath

local Nylon = require 'nylon.core'()
require 'NylonOs'


Nylon.cord( 'x', function(cord)
               while true do
                  NylonOs.Static.getclipboard_ext(
                     function(tyyp, content)
                        print( string.format( "%s Clipboard type=%s #=%d", os.date(), tyyp, #content ) )
                        if tyyp == 'CF_HDROP' then
                           for _, v in ipairs(content) do
                              print( 'lua found file=', v )
                           end
                        elseif tyyp == 'CFSTR_FILECONTENTS' then
                           local file = content[1]
                           print( string.format('found filecontents, gdrc=%s ole=%s flags=%s n=%s sz=%s tymed=%s',
                                                file.gdrc, file.haveOle, file.flags, file.name, file.size, file.tymed) )
                        end
                     end
                  )
                  cord:sleep(0.5)
               end
end)


Nylon.run()

        
