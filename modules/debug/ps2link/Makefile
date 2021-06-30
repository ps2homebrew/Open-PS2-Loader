IOP_BIN  = ps2link.irx
IOP_OBJS = net_fsys.o net_fio.o ps2link.o cmdHandler.o nprintf.o excepHandler.o imports.o

# Enable zero-copy on fileio writes.
ifeq ($(ZEROCOPY),1)
IOP_CFLAGS += -DZEROCOPY
endif

# Enable debug mode
ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

ifeq ($(PWOFFONRESET),1)
IOP_CFLAGS += -DPWOFFONRESET
endif

include $(PS2SDK)/Defs.make
include ../../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal
