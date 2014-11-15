

local maxpairs = Pdcurses.Static.color_pairs()

local assigned_pairs = {}
local free_pairs = {}
for i = 1,maxpairs do
   table.insert(free_pairs,i)
end

local wv = require 'nylon.debug'{ name = 'pls-theme' }

local function get_color( fg, bg )
   local key = fg * 0x10000 + bg
   if assigned_pairs[key] then
      return assigned_pairs[key][1]
   else
      local pair = free_pairs[#free_pairs]
      table.remove(free_pairs,#free_pairs)
      local color = Pdcurses.Color(pair, fg, bg)
      assigned_pairs[key] = { color, pair }
      wv.log('debug', 'add color theme, pair=%d fg=%d bg=%d',
             pair, fg, bg )
      return color
   end
end


local theme = {
   normal  = { Pdcurses.Color.white, Pdcurses.Color.black },
   inverse = { Pdcurses.Color.black, Pdcurses.Color.white },
}

return setmetatable( {
                        color = get_color
                     }, {
   index = function(t,v)
      local fgbg = theme[t]
      if fgbg then
         return get_color( fgbg[i], fgbg[2] )
      end
   end } )
