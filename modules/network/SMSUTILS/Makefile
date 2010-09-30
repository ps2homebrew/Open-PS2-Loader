
IOP_BIN = SMSUTILS.irx
IOP_OBJS = SMSUTILS.o

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_CFLAGS += -Wall -fno-builtin
IOP_LDFLAGS += -Wl,-s

all: $(IOP_BIN)

clean:
	-rm -f *.o *.irx

include $(PS2SDK)/Defs.make
include Rules.make
