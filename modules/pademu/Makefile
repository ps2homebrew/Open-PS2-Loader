IOP_OBJS = pademu.o sys_utils.o imports.o exports.o padmacro.o ds34common.o

IOP_BIN = pademu.irx

ifeq ($(USE_DS34USB),1)
IOP_CFLAGS += -DDS34USB
IOP_OBJS += ds34usb.o
endif

ifeq ($(USE_DS34BT),1)
IOP_CFLAGS += -DDS34BT
IOP_OBJS += ds34bt.o
endif

ifeq ($(VMC),1)
IOP_CFLAGS += -DVMC
endif

IOP_CFLAGS += -DUSE_SMSUTILS
IOP_INCS += -I../../include/

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal