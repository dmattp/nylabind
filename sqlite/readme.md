To get the sqlite stuff to build, you use Makefile.msc and you have to change DYNAMIC_SHELL from 0 to 1.

!IFNDEF DYNAMIC_SHELL
DYNAMIC_SHELL = 1
!ENDIF

Otherwise it just builds what looks like a standalone exe with no symbols exported from the DLL
