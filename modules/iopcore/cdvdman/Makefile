# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: $

IOP_OBJS = cdvdman.o streaming.o ioplib_util.o smsutils.o imports.o exports.o
USE_DEV9 ?= 0

ifeq ($(USE_HDD),1)
IOP_BIN  = hdd_cdvdman.irx
IOP_OBJS_DIR = obj.hdd/
IOP_OBJS += device-hdd.o atad.o
IOP_CFLAGS += -DHDD_DRIVER
USE_DEV9 = 1
endif

ifeq ($(USE_HDPRO),1)
IOP_BIN  = hdd_hdpro_cdvdman.irx
IOP_OBJS_DIR = obj.hdpro/
IOP_OBJS += device-hdd.o hdpro_atad.o
IOP_CFLAGS += -DHDD_DRIVER -DHD_PRO
ifeq ($(IOPCORE_DEBUG),1)
USE_DEV9 = 1
endif
endif

ifeq ($(USE_SMB),1)
IOP_BIN  = smb_cdvdman.irx
IOP_OBJS_DIR = obj.smb/
IOP_OBJS += device-smb.o smb.o
IOP_CFLAGS += -DSMB_DRIVER
USE_DEV9 = 1
IOP_INCS += -I../../network/common
endif

ifeq ($(USE_USB),1)
IOP_BIN  = usb_cdvdman.irx
IOP_OBJS_DIR = obj.usb/
IOP_OBJS += device-usb.o mass_driver.o
IOP_CFLAGS += -DUSB_DRIVER
ifeq ($(IOPCORE_DEBUG),1)
USE_DEV9 = 1
endif
endif


IOP_LIBS =
IOP_INCS += -I../common

IOP_CFLAGS += -Wall -fno-builtin-printf

ifeq ($(IOPCORE_DEBUG),1)
IOP_CFLAGS += -D__IOPCORE_DEBUG
endif

ifeq ($(USE_DEV9),1)
IOP_OBJS += dev9.o
IOP_CFLAGS += -D__USE_DEV9
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJS_DIR)%)

IOP_LDFLAGS += -s

all: OBJ_DIR $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) cdvdman.map

rebuild: clean all

OBJ_DIR:
	mkdir -p $(IOP_OBJS_DIR)

include $(PS2SDK)/Defs.make
include Rules.make
