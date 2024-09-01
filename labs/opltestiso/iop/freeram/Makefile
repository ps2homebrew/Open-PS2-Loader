IOP_BIN = freeram.irx
IOP_OBJS = main.o imports.o

IOP_CFLAGS += -Wall -Werror

ifeq ($(IOP_MEM_SIZE), 8)
  IOP_CFLAGS += -DIOP_MEM_8MB
endif

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
