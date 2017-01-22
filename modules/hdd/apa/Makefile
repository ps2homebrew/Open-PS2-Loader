LIBAPA_PATH = ../libapa
LIBAPA_SRC_DIR = $(LIBAPA_PATH)/src/

IOP_BIN  = ps2hdd.irx
APA_OBJS = misc.o cache.o apa.o journal.o free.o password.o
IOP_OBJS = hdd_fio.o hdd.o imports.o $(APA_OBJS)

IOP_INCS += -I$(CURDIR) -I$(LIBAPA_PATH)/include -I../common
IOP_CFLAGS  += -Wall -fno-builtin-printf
IOP_LDFLAGS += -s

all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

rebuild: clean all

$(IOP_OBJS_DIR)%.o : $(LIBAPA_SRC_DIR)%.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

include $(PS2SDK)/Defs.make
include ../../Rules.make
