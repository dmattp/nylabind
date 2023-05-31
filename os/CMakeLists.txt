
project(NylonOs)

# set(OS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/os-amalgamation-3400100)

add_library (NylonOs SHARED
  evid.cpp
  NylonOs.cpp)

set_target_properties(NylonOs PROPERTIES
  PREFIX ""
  INSTALL_RPATH "/usr/lib/lua/5.4/nylon")

include_directories(${NYLON_INCLUDE_DIRS})

set_target_properties(NylonSqlite PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

target_link_directories(NylonSqlite
  PRIVATE ${LUABIND_LINK_DIR}
  PRIVATE ${NYLON_LINK_DIR})

if (WIN32)
    target_link_libraries(NylonOs GdiPlus luabind syscore ${LUA_LIBRARIES})
else()
    target_link_libraries(NylonOs ${LUABIND_LINK_DIR}/libluabind.so ${NYLON_LINK_DIR}/syscore.so)
endif()

INSTALL(TARGETS NylonOs DESTINATION /usr/lib/lua/5.3/nylabind)
