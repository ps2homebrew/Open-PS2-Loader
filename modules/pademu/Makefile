
IOP_OBJS = pademu.o sys_utils.o imports.o exports.o
ifeq ($(USE_USB),1)
IOP_BIN = usb_pademu.irx
IOP_CFLAGS += -DUSB
IOP_OBJNAME = usb.
IOP_OBJS += ds3usb.o
endif
ifeq ($(USE_BT),1)
IOP_BIN = bt_pademu.irx
IOP_CFLAGS += -DBT
IOP_OBJNAME = bt.
IOP_OBJS += ds3bt.o
endif

IOP_OBJS := $(IOP_OBJS:%=$(IOP_OBJNAME)%)

IOP_CFLAGS += -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

include $(PS2SDK)/Defs.make
include Rules.make
