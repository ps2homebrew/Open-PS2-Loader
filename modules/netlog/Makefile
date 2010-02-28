IOP_BIN = netlog.irx
IOP_OBJS = netlog.o udpcl.o xprintf.o exports.o imports.o
IOP_LIBS =

IOP_INCS +=
IOP_CFLAGS += -Wall -fno-builtin
IOP_ASFLAGS +=
IOP_LDFLAGS += -s


all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include Rules.make
