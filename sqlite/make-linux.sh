#!/bin/sh
g++ -fPIC --shared --std=c++0x -I/usr/include/lua5.2 -I ../../nylon/src -I ../../nylon/src/linux-gtk NylonSqlite.cpp \
 -lsqlite3 -lluabind -llua5.2 ../../nylon/nylon/syscore.so -o ../bin/NylonSqlite.so
