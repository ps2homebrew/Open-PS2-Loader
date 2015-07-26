EE_BIN = elfldr.elf
EE_OBJS = elfldr.o crt0.o

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I.
EE_CFLAGS := -D_EE -O2 -mgpopt -G8192 -Wall $(EE_INCS)

EE_LDFLAGS = -nostartfiles -Tlinkfile -L$(PS2SDK)/ee/lib -s
EE_LIBS += -lkernel

%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS) -Xlinker -Map -Xlinker elfldr.map


all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) elfldr.map

include $(PS2SDK)/samples/Makefile.pref
