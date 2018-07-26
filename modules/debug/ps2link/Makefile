#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

IOP_BIN  = ps2link.irx
IOP_OBJS = net_fsys.o net_fio.o ps2link.o cmdHandler.o nprintf.o excepHandler.o imports.o

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_LIBS += 
IOP_LDFLAGS += -s
IOP_CFLAGS += -Wall -fno-builtin

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


all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include ../../Rules.make
