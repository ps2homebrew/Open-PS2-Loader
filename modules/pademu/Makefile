IOP_OBJS = pademu.o sys_utils.o imports.o exports.o padmacro.o ds34common.o

ifeq ($(USE_USB),1)
IOP_BIN = usb_pademu.irx
IOP_CFLAGS += -DUSB
IOP_OBJS_DIR = obj.usb/
IOP_OBJS += ds34usb.o
endif

# Xbox 360 (XInput) pads: same driver as the USB one plus the XInput code, loaded only when the "Xbox 360 XInput USB" mode is selected
ifeq ($(USE_X360),1)
IOP_BIN = x360_pademu.irx
IOP_CFLAGS += -DUSB -DX360
IOP_OBJS_DIR = obj.x360/
IOP_OBJS += ds34usb.o
endif

ifeq ($(USE_BT),1)
IOP_BIN = bt_pademu.irx
IOP_CFLAGS += -DBT
IOP_OBJS_DIR = obj.bt/
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
