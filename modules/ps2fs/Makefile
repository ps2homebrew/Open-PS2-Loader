IOP_BIN  = ps2fs.irx

IOP_INCS +=
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s

IOP_OBJS = bitmap.o dir.o inode.o journal.o misc.o pfs.o pfs_fio.o super.o cache.o block.o pfs_fioctl.o imports.o
IOP_LIBS += -lgcc

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)


include $(PS2SDK)/Defs.make
include ../Rules.make
