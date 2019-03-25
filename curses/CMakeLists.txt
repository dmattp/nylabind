# g++ -fPIC --shared --std=c++0x -I/usr/include/lua5.2 -I ../../nylon/src -I ../../nylon/src/linux-gtk LbindPdcurses.cpp -lcurses -lluabind -llua5.2 ../../nylon/nylon/syscore.so -o ../bin/LbindPdcurses.so


add_library (LbindPdcurses SHARED LbindPdcurses.cpp)

set_target_properties(LbindPdcurses PROPERTIES PREFIX "")
target_link_libraries(LbindPdcurses luabind /usr/lib/lua/5.3/nylon/syscore.so)

INSTALL(TARGETS LbindPdcurses DESTINATION /usr/lib/lua/5.3/nylabind)
