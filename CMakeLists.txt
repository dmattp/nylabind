cmake_minimum_required (VERSION 3.13)

project (NYLABIND)

set(CMAKE_BUILD_TYPE Debug)

include_directories(/usr/local/include/nylon)

add_subdirectory(curses)
add_subdirectory(sqlite)
add_subdirectory(zmq)


