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

ifeq ($(USE_HDD),1)
IOP_BIN  = hdd_cdvdman.irx
IOP_OBJNAME = hdd.
IOP_OBJS += device-hdd.o dev9.o atad.o
endif

ifeq ($(USE_HDPRO),1)
IOP_BIN  = hdd_hdpro_cdvdman.irx
IOP_OBJNAME = hdpro.
IOP_OBJS += device-hdd.o hdpro_atad.o
endif

ifeq ($(USE_SMB),1)
IOP_BIN  = smb_cdvdman.irx
IOP_OBJNAME = smb.
IOP_OBJS += device-smb.o dev9.o smb.o
endif

ifeq ($(USE_USB),1)
IOP_BIN  = usb_cdvdman.irx
IOP_OBJNAME = usb.
IOP_OBJS += device-usb.o mass_driver.o
ifeq ($(USE_DEV9),1)
IOP_OBJS += dev9.o
else
ifeq ($(IOPCORE_DEBUG),1)
IOP_OBJS += dev9.o
endif
endif
endif


IOP_LIBS =
IOP_INCS = -I../common

ifeq ($(USE_SMB),1)
IOP_INCS += -I../../network/common
endif

IOP_CFLAGS += -Wall -fno-builtin-printf

ifeq ($(USE_HDD),1)
IOP_CFLAGS += -DHDD_DRIVER
endif

ifeq ($(USE_HDPRO),1)
IOP_CFLAGS += -DHDD_DRIVER -DHD_PRO
endif

ifeq ($(USE_SMB),1)
IOP_CFLAGS += -DSMB_DRIVER
endif

ifeq ($(USE_USB),1)
IOP_CFLAGS += -DUSB_DRIVER
endif

ifeq ($(IOPCORE_DEBUG),1)
IOP_CFLAGS += -D__IOPCORE_DEBUG
endif

ifeq ($(USE_HDPRO),1)
ifeq ($(IOPCORE_DEBUG),1)
IOP_OBJS += dev9.o
endif
endif
ifeq ($(USE_USB),1)
ifeq ($(USE_DEV9),1)
IOP_CFLAGS += -D__USE_DEV9
else
ifeq ($(IOPCORE_DEBUG),1)
IOP_CFLAGS += -D__USE_DEV9
endif
endif
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC_DRIVER
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJNAME)%)

IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS) cdvdman.map

rebuild: clean all

include $(PS2SDK)/Defs.make
include Rules.make
