# g++ -fPIC --shared --std=c++0x -I/usr/include/lua5.2 -I ../../nylon/src -I ../../nylon/src/linux-gtk LbindPdcurses.cpp -lcurses -lluabind -llua5.2 ../../nylon/nylon/syscore.so -o ../bin/LbindPdcurses.so

project(NylonSqlite)

set(SQLITE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sqlite-amalgamation-3400100)

add_library (NylonSqlite SHARED
  sqlite-amalgamation-3400100/sqlite3.c
  NylonSqlite.cpp)

set_target_properties(NylonSqlite PROPERTIES
  PREFIX ""
  INSTALL_RPATH "/usr/lib/lua/5.4/nylon")


include_directories(${NYLON_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIR})

set_target_properties(NylonSqlite PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

##target_link_directories(NylonSqlite PRIVATE ${LUABIND_LINK_DIR})
##target_link_directories(NylonSqlite PRIVATE ${NYLON_LINK_DIR})

if (WIN32)
    target_link_libraries(NylonSqlite luabind syscore ${LUA_LIBRARIES})
else()
    target_link_libraries(NylonSqlite ${LUABIND_LINK_DIR}/libluabind.so ${NYLON_LINK_DIR}/syscore.so)
endif()
  
#target_link_libraries(NylonSqlite luabind :syscore.so ${LUA_LIBRARIES})

INSTALL(TARGETS NylonSqlite DESTINATION /usr/lib/lua/5.3/nylabind)
