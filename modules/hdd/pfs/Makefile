LIBPFS_PATH = ../libpfs
LIBPFS_SRC_DIR = $(LIBPFS_PATH)/src/

IOP_BIN  = ps2fs.irx
PFS_OBJS = bitmap.o dir.o inode.o journal.o misc.o super.o superWrite.o cache.o block.o blockWrite.o
IOP_OBJS = pfs.o pfs_fio.o pfs_fioctl.o imports.o $(PFS_OBJS)

IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s
IOP_INCS += -I$(CURDIR) -I$(LIBPFS_PATH)/include -I../common
IOP_LIBS += -lgcc

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

$(IOP_OBJS_DIR)%.o : $(LIBPFS_SRC_DIR)%.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

include $(PS2SDK)/Defs.make
include ../../Rules.make
