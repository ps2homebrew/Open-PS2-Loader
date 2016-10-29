
IOP_BIN = SMSUTILS.irx
IOP_OBJS = SMSUTILS.o

IOP_INCS += -I$(PS2SDK)/iop/include -I../../iopcore/common
IOP_CFLAGS += -Wall -fno-builtin
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../../Rules.make
