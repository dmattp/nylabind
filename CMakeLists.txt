cmake_minimum_required (VERSION 3.10)
set(CMAKE_GENERATOR_TOOLSET v141_xp)

project (NYLABIND)
set(NYLABIND_VERSION_MAJOR 0)
set(NYLABIND_VERSION_MINOR 1)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(NYLON_INCLUDE "${CMAKE_SOURCE_DIR}/../nylon/src" "${CMAKE_SOURCE_DIR}/../nylon/src/win/sys")
  set(LUABIND_INCLUDE "${CMAKE_SOURCE_DIR}/../nylon/luabind-deboostified")
else()
  set(NYLON_INCLUDE /usr/local/include/nylon)
endif()

include_directories(${NYLON_INCLUDE} ${LUABIND_INCLUDE})

add_subdirectory(curses)
add_subdirectory(sqlite)
add_subdirectory(zmq)


