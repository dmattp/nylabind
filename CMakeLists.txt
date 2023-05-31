# The name of our project is "HELLO". CMakeLists files in this project can 
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and 
# to the root binary directory of the project as ${HELLO_BINARY_DIR}. 
# Useful variables: http://www.cmake.org/Wiki/CMake_Useful_Variables

# to build, do this:
# mkdir build; cd build; cmake .. && cmake --build . && cp ./src/syscore.so ../nylon/ && ctest

cmake_minimum_required (VERSION 3.0) 

project (NYLABIND)

set(CMAKE_CXX_STANDARD 17)

if(NOT LUA_FOUND AND NOT LUA54_FOUND)
	find_package(Lua54 REQUIRED)
	set(LUA_INCLUDE_DIRS "${LUA_INCLUDE_DIR}")
endif()

set(NYLON_DEV_DIR "${CMAKE_SOURCE_DIR}/../boostless-nylon")

set(LUABIND_INCLUDE_DIR "${NYLON_DEV_DIR}/luabind-deboostified")
set(NYLON_INCLUDE_DIR   "${NYLON_DEV_DIR}/src")

if (WIN32)
  set(NYLON_SYS_INCLUDE_DIR "${NYLON_INCLUDE_DIR}/win")
  set(CONVENTION_BUILD_DIR "build-win")
  set(CONVENTION_LIB_SUBDIR "/Release")
else()
  set(NYLON_SYS_INCLUDE_DIR "${NYLON_INCLUDE_DIR}/linux-gtk")
  set(CONVENTION_BUILD_DIR "build")
  set(CONVENTION_LIB_SUBDIR "")
endif()

set(NYLON_INCLUDE_DIRS ${LUA_INCLUDE_DIR} ${LUABIND_INCLUDE_DIR} ${NYLON_INCLUDE_DIR} ${NYLON_SYS_INCLUDE_DIR})

set(LUABIND_LINK_DIR "${NYLON_DEV_DIR}/${CONVENTION_BUILD_DIR}/lib${CONVENTION_LIB_SUBDIR}")
set(NYLON_LINK_DIR "${NYLON_DEV_DIR}/${CONVENTION_BUILD_DIR}/src${CONVENTION_LIB_SUBDIR}")


link_directories(${LUABIND_LINK_DIR} ${NYLON_LINK_DIR})

#set(LUA_LINK_DIR "${LUA_INCLUDE}/${CONVENTION_BUILD_DIR}/src${CONVENTION_LIB_DIR}")

add_subdirectory(sqlite)
add_subdirectory(os)


#enable_testing()
#add_test( NAME all-tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests COMMAND lua run-all-tests.lua)
