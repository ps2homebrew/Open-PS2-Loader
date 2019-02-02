# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_CC_VERSION := $(shell $(IOP_CC) --version 2>&1 | sed -n 's/^.*(GCC) //p')

ifeq ($(IOP_CC_VERSION),3.2.2)
ASFLAGS_TARGET = -march=r3000
endif

ifeq ($(IOP_CC_VERSION),3.2.3)
ASFLAGS_TARGET = -march=r3000
endif

# include dir
IOP_INCS := $(IOP_INCS) -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include -Iinclude

# C compiler flags
# -fno-builtin is required to prevent the GCC built-in functions from being included,
#   for finer-grained control over what goes into each IRX.
IOP_CFLAGS  := -D_IOP -fno-builtin -Os -G0 $(IOP_INCS) $(IOP_CFLAGS)
# linker flags
IOP_LDFLAGS := -nostdlib $(IOP_LDFLAGS)
#IOP_LDFLAGS := -nostdlib -L$(PS2SDK)/iop/lib $(IOP_LDFLAGS)

# Additional C compiler flags for GCC >=v5.3.0
# -msoft-float is to "remind" GCC/Binutils that the soft-float ABI is to be used. This is due to a bug, which
#   results in the ABI not being passed correctly to binutils and iop-as defaults to the hard-float ABI instead.
# -mno-explicit-relocs is required to work around the fact that GCC is now known to
#   output multiple LO relocs after one HI reloc (which the IOP kernel cannot deal with).
# -fno-toplevel-reorder (for IOP import and export tables only) disables toplevel reordering by GCC v4.2 and later.
#   Without it, the import and export tables can be broken apart by GCC's optimizations.
ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
IOP_CFLAGS += -msoft-float -mno-explicit-relocs
IOP_IETABLE_CFLAGS := -fno-toplevel-reorder
endif
endif

# Assembler flags
IOP_ASFLAGS := $(ASFLAGS_TARGET) -EL -G0 $(IOP_ASFLAGS)

BIN2C = $(PS2SDK)/bin/bin2c
BIN2S = $(PS2SDK)/bin/bin2s
BIN2O = $(PS2SDK)/bin/bin2o

# Externally defined variables: IOP_BIN, IOP_OBJS, IOP_LIB

$(IOP_OBJS_DIR)%.o : %.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

$(IOP_OBJS_DIR)%.o : %.S
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

$(IOP_OBJS_DIR)%.o : %.s
	$(IOP_AS) $(IOP_ASFLAGS) $< -o $@

# A rule to build imports.lst.
$(IOP_OBJS_DIR)%.o : %.lst
	$(ECHO) "#include \"irx_imports.h\"" > $(IOP_OBJS_DIR)build-imports.c
	cat $< >> $(IOP_OBJS_DIR)build-imports.c
	$(IOP_CC) $(IOP_CFLAGS) -I. -c $(IOP_OBJS_DIR)build-imports.c -o $@
	-rm -f $(IOP_OBJS_DIR)build-imports.c

# A rule to build exports.tab.
$(IOP_OBJS_DIR)%.o : %.tab
	$(ECHO) "#include \"irx.h\"" > $(IOP_OBJS_DIR)build-exports.c
	cat $< >> $(IOP_OBJS_DIR)build-exports.c
	$(IOP_CC) $(IOP_CFLAGS) -I. -c $(IOP_OBJS_DIR)build-exports.c -o $@
	-rm -f $(IOP_OBJS_DIR)build-exports.c

$(IOP_BIN) : $(IOP_OBJS)
	$(IOP_CC) $(IOP_CFLAGS) -o $(IOP_BIN) $(IOP_OBJS) $(IOP_LDFLAGS) $(IOP_LIBS)

$(IOP_LIB) : $(IOP_OBJS)
	$(IOP_AR) cru $(IOP_LIB) $(IOP_OBJS)
