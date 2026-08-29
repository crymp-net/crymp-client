INCLUDE_FLAGS	= -I../../CryCommon -I../../Win32APIWrapper 
WARNING_FLAGS	= -w
#-Wall -W -Wno-unused -Wno-uninitialized
VPATH			= 
LIBS			= 
SYSTEM_LIBS	=

FILES =	$(wildcard *.c)

OUTPUT		= zlib
OUTPUT_TYPE = incr_obj

all: 		release
allrelease:	release
alldebug: 	debug


#special for this lib
CXX = pu-lv2-gcc-3.4.1
CXXFLAGS_OPT_LEVEL = -O3

include ../../Makefile.common
