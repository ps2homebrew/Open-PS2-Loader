# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: $

IOP_OBJS = mcemu.o mcemu_io.o mcemu_sys.o mcemu_var.o mcemu_rpc.o imports.o

ifeq ($(USE_HDD),1)
IOP_BIN  = hdd_mcemu.irx
IOP_OBJS_DIR = obj.hdd/
IOP_OBJS += device-hdd.o
IOP_CFLAGS += -DHDD_DRIVER
endif

ifeq ($(USE_SMB),1)
IOP_BIN  = smb_mcemu.irx
IOP_OBJS_DIR = obj.smb/
IOP_OBJS += device-smb.o
IOP_CFLAGS += -DSMB_DRIVER
endif

ifeq ($(USE_USB),1)
IOP_BIN  = usb_mcemu.irx
IOP_OBJS_DIR = obj.usb/
IOP_OBJS += device-usb.o
IOP_CFLAGS += -DUSB_DRIVER
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJS_DIR)%)

ifeq ($(IOPCORE_DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

IOP_CFLAGS += -Wall -fno-builtin

ifeq ($(PADEMU),1)
IOP_CFLAGS += -DPADEMU
endif

IOP_LDFLAGS += -s
IOP_INCS +=

all: OBJ_DIR $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

rebuild: clean all

OBJ_DIR:
	mkdir -p $(IOP_OBJS_DIR)

include $(PS2SDK)/Defs.make
include Rules.make
