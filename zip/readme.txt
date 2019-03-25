getting libzip cmake to work with zlib is a mess.

Had to add:

cmake_policy(SET CMP0074 NEW)

because  the find zlib module uses ZLIB_ROOT, but that's not allowed unless
you explictly set the policy to new?  It's very weird.

also had to copy zconf.h from zlib's build directory to the same root
directory with zlib.h, since there is, i guess, only one zlib include
directory (possibly running some sort of install on zlib would put these two
together?)

