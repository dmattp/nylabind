# g++ -fPIC --shared --std=c++0x -I/usr/include/lua5.2 -I ../../nylon/src -I ../../nylon/src/linux-gtk LbindPdcurses.cpp -lcurses -lluabind -llua5.2 ../../nylon/nylon/syscore.so -o ../bin/LbindPdcurses.so

add_library (NylonSqlite SHARED NylonSqlite.cpp)

set_target_properties(NylonSqlite PROPERTIES
  PREFIX ""
  INSTALL_RPATH "/usr/lib/lua/5.3/nylon"
  )
target_link_libraries(NylonSqlite luabind sqlite3 /usr/lib/lua/5.3/nylon/syscore.so)

INSTALL(TARGETS NylonSqlite DESTINATION /usr/lib/lua/5.3/nylabind)
