IOP_OBJS = cdvdman.o ioops.o ncmd.o scmd.o searchfile.o streaming.o ioplib_util.o smsutils.o imports.o exports.o ../../isofs/zso.o ../../isofs/lz4.o
USE_DEV9 ?= 0

ifeq ($(USE_HDD),1)
IOP_BIN  = hdd_cdvdman.irx
IOP_OBJS_DIR = obj.hdd/
IOP_OBJS += device-hdd.o atad.o
IOP_CFLAGS += -DHDD_DRIVER
USE_DEV9 = 1
endif

ifeq ($(USE_HDPRO),1)
IOP_BIN  = hdd_hdpro_cdvdman.irx
IOP_OBJS_DIR = obj.hdpro/
IOP_OBJS += device-hdd.o hdpro_atad.o
IOP_CFLAGS += -DHDD_DRIVER -DHD_PRO
ifeq ($(IOPCORE_DEBUG),1)
USE_DEV9 = 1
endif
endif

ifeq ($(USE_SMB),1)
IOP_BIN  = smb_cdvdman.irx
IOP_OBJS_DIR = obj.smb/
IOP_OBJS += device-smb.o smb.o
IOP_CFLAGS += -DSMB_DRIVER
USE_DEV9 = 1
IOP_INCS += -I../../network/common
endif

ifeq ($(USE_BDM),1)
IOP_BIN  = bdm_cdvdman.irx
IOP_OBJS_DIR = obj.bdm/
IOP_OBJS += device-bdm.o
IOP_CFLAGS += -DBDM_DRIVER
IOP_LIBS += -L$(PS2SDK)/iop/lib -lbdm
ifeq ($(IOPCORE_DEBUG),1)
USE_DEV9 = 1
endif
endif

IOP_INCS += -I../common

ifeq ($(IOPCORE_DEBUG),1)
IOP_CFLAGS += -D__IOPCORE_DEBUG
endif

ifeq ($(USE_DEV9),1)
IOP_OBJS += dev9.o
IOP_CFLAGS += -D__USE_DEV9
endif

include $(PS2SDK)/Defs.make
include ../../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal
