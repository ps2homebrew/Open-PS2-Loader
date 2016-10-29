IOP_BIN  = smbinit.irx
IOP_OBJS = main.o smbauth.o des.o md4.o imports.o

IOP_INCS += -I../../iopcore/common
IOP_CFLAGS += -Wall -fno-builtin -Os
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../../Rules.make
