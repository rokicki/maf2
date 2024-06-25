[NMAKE]
# default definitions and rules for the Win32 makefile for the DP4 system
###############################################################################

SLASH = \ #

# map file = change this to 'con' or '$*.map' if required
M = nul

# suffixes for object files
O = o32
W = v32
RO = rbj

# Tools
LIB32 = link.exe -lib
RC32 = rc.exe

!ifdef DEBUG
#To get Debug
CC = c:\progra~1\intel\compil~1\bin\icl.exe -Zi
LINK32 = link.exe -debug -pdb:none
COMMON = -nologo -c -I. -J -GF -Od -Gy -W4 -Fo$@ 
!else
#Normal
CC = c:\progra~1\intel\compil~1\bin\icl.exe
LINK32 = link.exe 
COMMON = -nologo -c -I. -J -GF -Ox -W4 -Fo$@ 
!endif

#flags
PFLAGS = -DWIN32 -D_WIN32_WINNT=0x501 -DWINVER=0x501 -D_X86_
CFLAGS = $(COMMON) -Za
CPPFLAGS = $(CFLAGS) -GR- -GX-
CWFLAGS = $(PFLAGS) $(COMMON) -Ze
#To get assembler
#CFLAGS = -nologo -c -DWIN32 -D_X86_ -I. -J -GF -Oxs -W4 -Zp1 -Fo$@ -FAsc -Fa

DEXTRA =

# files always required for the Win32 linker

L32_ALWAYS = -link -align:0x1000 -entry:mainCRTStartup -subsystem:windows,4.0 libc.lib kernel32.lib user32.lib gdi32.lib

# default suffixes

.SUFFIXES:
.SUFFIXES: .$O .$W .cpp .c .rbj .rc

# default rules


.c.$O:
	$(CC) $(CFLAGS) $*.c
.cpp.$O:
	$(CC) $(CPPFLAGS) $*.cpp

.c.$W:
	$(CC) $(CWFLAGS) $*.c

.cpp.$W:
	$(CC) $(CWFLAGS) $*.cpp

.rc.rbj:
	$(RC) $(PFLAGS) -Fo$*.rbj $*.rc

# first entry in makefile will make appropriate targets for the
# host operating system; this macro sets the choice to 'win32'
ALL = win32
CFG= Win32 Release
