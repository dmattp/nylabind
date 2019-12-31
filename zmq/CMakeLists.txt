# g++ -fPIC --shared --std=c++0x -I/usr/include/lua5.2 -I ../../nylon/src -I ../../nylon/src/linux-gtk LbindPdcurses.cpp -lcurses -lluabind -llua5.2 ../../nylon/nylon/syscore.so -o ../bin/LbindPdcurses.so

add_library (zmqnylon SHARED zmqnylon.cpp)

set_target_properties(zmqnylon PROPERTIES
  PREFIX ""
  INSTALL_RPATH "/usr/lib/lua/5.3/nylon"
  )
target_link_libraries(zmqnylon luabind zmq /usr/lib/lua/5.3/nylon/syscore.so)

INSTALL(TARGETS zmqnylon DESTINATION /usr/lib/lua/5.3/nylabind)
