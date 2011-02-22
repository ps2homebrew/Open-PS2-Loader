IOP_BIN = SMSMAP.irx
IOP_OBJS = main.o smap_opt.o smap.o imports.o
IOP_LIBS =

IOP_INCS +=
IOP_CFLAGS += -Wall -fno-builtin -DFORCE_100M_FD
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
