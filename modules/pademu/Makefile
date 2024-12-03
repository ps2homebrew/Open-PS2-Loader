IOP_OBJS = pademu_common.o pademu.o sys_utils.o imports.o exports.o padmacro.o

IOP_BIN = pademu.irx

ifeq ($(USE_DS3),1)
IOP_CFLAGS += -DUSE_DS3
IOP_OBJS += ds3usb.o ds3bt.o
endif

ifeq ($(USE_DS4),1)
IOP_CFLAGS += -DUSE_DS4
IOP_OBJS += ds4usb.o ds4bt.o
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_CFLAGS += -DUSE_SMSUTILS
IOP_INCS += -I../../include/

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal