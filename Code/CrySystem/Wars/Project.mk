#############################################################################
## Crytek Source File
## Copyright (C) 2006, Crytek Studios
##
## Creator: Sascha Demetrio
## Date: Jul 31, 2006
## Description: GNU-make based build system
#############################################################################

PROJECT_TYPE := module
PROJECT_VCPROJ := CrySystem.vcproj

PROJECT_PS3_PRX := 1
ifeq ($(ARCH),PS3-cell)
 PROJECT_STUB_SOURCES_CPP := DllMain.cpp CryMemoryManager.cpp
endif

PROJECT_LINKMODULES := \
	XRenderD3D10 \
	CryNetwork \
	CryEntitySystem \
	CryInput \
	CrySoundSystem \
	CryMovie \
	CryAISystem \
	CryScriptSystem \
	Cry3DEngine \
	CryAnimation \
	CryFont \
	CryPhysics

PROJECT_CPPFLAGS := \
	-I$(CODE_ROOT)/CryEngine/CryCommon \
	-I$(CODE_ROOT)/SDKs/Scaleform/Include \
	-I$(CODE_ROOT)/SDKs/PunkBuster \
	-I$(CODE_ROOT)/SDKs \
	-I$(PROJECT_CODE)/XML/Expat \
	-I$(PROJECT_CODE)/zlib

include $(MAKE_ROOT)/Lib/ps3prxdefs.mk

ifeq ($(OPTION_PROFILE),1)
 ifeq ($(OPTION_WHOLE_PROJECT),1)
  PROJECT_XFLAGS := -O3 -fno-schedule-insns2 -fno-schedule-insns 
 else
  PROJECT_XFLAGS := -O3
  PROJECT_XFLAGS_CryMemoryManager_cpp := \
	-O2 -fno-schedule-insns2 -fno-schedule-insns
 endif
endif

ifeq ($(ARCH),PS3-cell)
 PROJECT_LDFLAGS := \
	-L$(CODE_ROOT)/SDKs/Scaleform/Lib \
	-L$(CODE_ROOT)/Tools/PS3JobManager/lib
 PROJECT_LDLIBS := \
	-lGFx_opt_nortti -ljpeg \
	-lcryjobmanspu \
	-lm_stub -lfs_stub -lrtc_stub -lnet_stub -lpthread
endif

PROJECT_SOURCES_CPP_REMOVE := StdAfx.cpp

# vim:ts=8:sw=8

