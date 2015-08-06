IOP_BIN  = udptty.irx

IOP_INCS += -I../../network/common
IOP_CFLAGS += -Wall -fno-builtin-printf -DKPRTTY
IOP_LDFLAGS += -s

IOP_OBJS = udptty.o imports.o exports.o
IOP_LIBS =

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../../Rules.make
