BIN2C = $(PS2SDK)/bin/bin2c
IOP_SRC_DIR = ./

all: $(IOP_BIN)

clean:
	rm -r -f $(IOP_BIN) $(IOP_OBJS) $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_LIB_DIR)

rebuild: clean all

run:
	ps2client -t 1 execiop host:$(IOP_BIN)
