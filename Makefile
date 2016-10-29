VERSION = 0
SUBVERSION = 9
PATCHLEVEL = 3+
EXTRAVERSION = Beta

# How to DEBUG?
# Simply type "make <debug mode>" to build OPL with the necessary debugging functionality.
# Debug modes:
#	debug		-	UI-side debug mode (UDPTTY)
#	iopcore_debug	-	UI-side + iopcore debug mode (UDPTTY).
#	ingame_debug	-	UI-side + in-game debug mode. IOP core modules will not be built as debug versions (UDPTTY).
#	eesio_debug	-	UI-side + eecore debug mode (EE SIO)
#	deci2_debug	-	UI-side + in-game DECI2 debug mode (EE-side only).

# I want to build a CHILDPROOF edition! How do I do that?
# Type "make childproof" to build one.
# Non-childproof features like GSM will not be available.

# I want to put my name in my custom build! How i can do it?
# Type "make LOCALVERSION=-foobar"

# ======== START OF CONFIGURABLE SECTION ========
# You can adjust the variables in this section to meet your needs.
# To enable a feature, set its variable's value to 1. To disable, change it to 0.
# Do not COMMENT out the variables!!

#Enables/disables Virtual Memory Card (VMC) support
VMC = 0

#Enables/disables Right-To-Left (RTL) language support
RTL = 0

#Enables/disables Graphics Synthesizer Mode (GSM) selector
GSM = 0

#Enables/disables In Game Screenshot (IGS). NB: It depends on GSM and IGR to work
IGS = 0

#Enables/disables the cheat engine (PS2RD)
CHEAT = 0

#Enables/disables building of an edition of OPL that will support the DTL-T10000 (SDK v2.3+)
DTL_T10000 = 0

#Nor stripping neither compressing binary ELF after compiling.
NOT_PACKED = 0

# ======== END OF CONFIGURABLE SECTION. DO NOT MODIFY VARIABLES AFTER THIS POINT!! ========
DEBUG = 0
EESIO_DEBUG = 0
INGAME_DEBUG = 0
DECI2_DEBUG = 0
CHILDPROOF = 0

# ======== DO NOT MODIFY VALUES AFTER THIS POINT! UNLESS YOU KNOW WHAT YOU ARE DOING ========
REVISION = $(shell expr $$(cat DETAILED_CHANGELOG | grep "rev" | head -1 | cut -d " " -f 1 | cut -c 4-) + 1)

GIT_HASH = $(shell git rev-parse --short=7 HEAD 2>/dev/null)
ifeq ($(shell git diff --quiet; echo $$?),1)
  DIRTY = -dirty
endif
ifneq ($(shell test -d .git; echo $?),0)
  DIRTY = -dirty
endif

OPL_VERSION = $(VERSION).$(SUBVERSION).$(PATCHLEVEL).$(REVISION)$(if $(EXTRAVERSION),-$(EXTRAVERSION))$(if $(GIT_HASH),-$(GIT_HASH))$(if $(DIRTY),-$(DIRTY))$(if $(LOCALVERSION),-$(LOCALVERSION))

FRONTEND_OBJS = obj/pad.o obj/fntsys.o obj/renderman.o obj/menusys.o obj/OSDHistory.o obj/system.o obj/lang.o obj/config.o obj/hdd.o obj/dialogs.o \
		obj/dia.o obj/ioman.o obj/texcache.o obj/themes.o obj/supportbase.o obj/usbsupport.o obj/ethsupport.o obj/hddsupport.o \
		obj/appsupport.o obj/gui.o obj/textures.o obj/opl.o obj/atlas.o obj/nbns.o obj/httpclient.o

GFX_OBJS =	obj/usb_icon.o obj/hdd_icon.o obj/eth_icon.o obj/app_icon.o \
		obj/cross_icon.o obj/triangle_icon.o obj/circle_icon.o obj/square_icon.o obj/select_icon.o obj/start_icon.o \
		obj/left_icon.o obj/right_icon.o obj/up_icon.o obj/down_icon.o obj/L1_icon.o obj/L2_icon.o obj/R1_icon.o obj/R2_icon.o \
		obj/load0.o obj/load1.o obj/load2.o obj/load3.o obj/load4.o obj/load5.o obj/load6.o obj/load7.o obj/logo.o obj/bg_overlay.o obj/freesans.o \
		obj/icon_sys.o obj/icon_icn.o

MISC_OBJS =	obj/icon_sys_A.o obj/icon_sys_J.o

IOP_OBJS =	obj/iomanx.o obj/filexio.o obj/ps2fs.o obj/usbd.o obj/usbhdfsd.o obj/usbhdfsdfsv.o \
		obj/ps2atad.o obj/hdpro_atad.o obj/poweroff.o obj/ps2hdd.o obj/genvmc.o obj/hdldsvr.o \
		obj/ps2dev9.o obj/smsutils.o obj/ps2ip.o obj/smap.o obj/isofs.o obj/nbns-iop.o \
		obj/httpclient-iop.o obj/netman.o obj/ps2ips.o

EECORE_OBJS = obj/ee_core.o obj/ioprp.o obj/util.o \
		obj/elfldr.o obj/udnl.o obj/imgdrv.o obj/eesync.o \
		obj/usb_cdvdman.o obj/IOPRP_img.o obj/smb_cdvdman.o \
		obj/hdd_cdvdman.o obj/hdd_hdpro_cdvdman.o obj/cdvdfsv.o \
		obj/ingame_smstcpip.o obj/smap_ingame.o obj/smbman.o obj/smbinit.o

EE_BIN = opl.elf
EE_BIN_PKD = OPNPS2LD.ELF
EE_VPKD = OPNPS2LD-$(OPL_VERSION)
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_OBJS = $(FRONTEND_OBJS) $(GFX_OBJS) $(MISC_OBJS) $(EECORE_OBJS) $(IOP_OBJS)

MAPFILE = opl.map
EE_LDFLAGS += -Wl,-Map,$(MAPFILE)
 
EE_LIBS = -L$(PS2SDK)/ports/lib -L$(GSKIT)/lib -L./lib -lgskit -ldmakit -lgskit_toolkit -lpoweroff -lfileXio -lpatches -ljpeg -lpng -lz -ldebug -lm -lmc -lfreetype -lvux -lcdvd -lnetman -lps2ips
EE_INCS += -I$(PS2SDK)/ports/include -I$(GSKIT)/include -I$(GSKIT)/ee/dma/include -I$(GSKIT)/ee/gs/include -I$(GSKIT)/ee/toolkit/include -Imodules/iopcore/common -Imodules/network/common -Imodules/hdd/common -Iinclude

BIN2C = $(PS2SDK)/bin/bin2c
BIN2S = $(PS2SDK)/bin/bin2s
BIN2O = $(PS2SDK)/bin/bin2o

# WARNING: Only extra spaces are allowed and ignored at the beginning of the conditional directives (ifeq, ifneq, ifdef, ifndef, else and endif)
# but a tab is not allowed; if the line begins with a tab, it will be considered part of a recipe for a rule! 

ifeq ($(VMC),1)
  IOP_OBJS += obj/usb_mcemu.o obj/hdd_mcemu.o obj/smb_mcemu.o 
  EE_CFLAGS += -DVMC
  VMC_FLAGS = VMC=1
else
  VMC_FLAGS = VMC=0
endif

ifeq ($(RTL),1)
  EE_CFLAGS += -D__RTL
endif

ifeq ($(DTL_T10000),1)
  EE_CFLAGS += -D_DTL_T10000
  EECORE_EXTRA_FLAGS += DTL_T10000=1
  IOP_OBJS += obj/sio2man.o obj/padman.o obj/mcman.o obj/mcserv.o
  EE_LIBS += -lpadx
else
  EE_LIBS += -lpad
endif

ifeq ($(CHILDPROOF),1)
  EE_CFLAGS += -D__CHILDPROOF
  GSM_FLAGS = GSM=0
  IGS_FLAGS = IGS=0
  CHEAT_FLAGS = CHEAT=0
else
  ifeq ($(IGS),1)
    GSM = 1
  endif
  ifeq ($(GSM),1)
    EE_CFLAGS += -DGSM
    GSM_FLAGS = GSM=1
    ifeq ($(IGS),1)
      EE_CFLAGS += -DIGS
      IGS_FLAGS = IGS=1
    else
      IGS_FLAGS = IGS=0
    endif
  else
    GSM_FLAGS = GSM=0
    IGS_FLAGS = IGS=0
  endif
  ifeq ($(CHEAT),1)
    FRONTEND_OBJS += obj/cheatman.o 
    EE_CFLAGS += -DCHEAT
    CHEAT_FLAGS = CHEAT=1
  else
    CHEAT_FLAGS = CHEAT=0
  endif
endif

ifeq ($(DEBUG),1) 
  EE_CFLAGS += -D__DEBUG -g
  EE_OBJS += obj/debug.o obj/udptty.o obj/ioptrap.o obj/ps2link.o
  MOD_DEBUG_FLAGS = DEBUG=1
  ifeq ($(IOPCORE_DEBUG),1)
    EE_CFLAGS += -D__INGAME_DEBUG
    EECORE_EXTRA_FLAGS = LOAD_DEBUG_MODULES=1
    CDVDMAN_DEBUG_FLAGS = IOPCORE_DEBUG=1
    MCEMU_DEBUG_FLAGS = IOPCORE_DEBUG=1
    SMSTCPIP_INGAME_CFLAGS = 
    IOP_OBJS += obj/udptty-ingame.o
  else ifeq ($(EESIO_DEBUG),1)
    EE_CFLAGS += -D__EESIO_DEBUG
    EECORE_EXTRA_FLAGS += EESIO_DEBUG=1
  else ifeq ($(INGAME_DEBUG),1)
    EE_CFLAGS += -D__INGAME_DEBUG
    EECORE_EXTRA_FLAGS = LOAD_DEBUG_MODULES=1
    CDVDMAN_DEBUG_FLAGS = USE_DEV9=1
    SMSTCPIP_INGAME_CFLAGS = 
    ifeq ($(DECI2_DEBUG),1)
      EE_CFLAGS += -D__DECI2_DEBUG
      EECORE_EXTRA_FLAGS += DECI2_DEBUG=1
      IOP_OBJS += obj/drvtif_irx.o obj/tifinet_irx.o
      DECI2_DEBUG=1
    else
      IOP_OBJS += obj/udptty-ingame.o
    endif
  endif
else
  EE_CFLAGS += -O2
  SMSTCPIP_INGAME_CFLAGS = INGAME_DRIVER=1
endif

EE_CFLAGS += -DOPL_VERSION=\"$(OPL_VERSION)\"

.SILENT:
all:
	@mkdir -p obj
	@mkdir -p asm
	
	echo "Building Open PS2 Loader $(OPL_VERSION)..."
	echo "-Interface"
	$(MAKE) $(EE_BIN)
	
ifneq ($(DEBUG),1)
  ifneq ($(NOT_PACKED),1)
	echo "Stripping..."
	ee-strip $(EE_BIN)

	echo "Compressing..."
	ps2-packer $(EE_BIN) $(EE_BIN_PKD) > /dev/null

    ifeq ($(RELEASE),1)
	cp $(EE_BIN_PKD) $(EE_VPKD).ELF
	zip -r $(EE_VPKD).zip $(EE_VPKD).ELF CREDITS *DETAILED_CHANGELOG LICENSE README
	echo "Package Complete: $(EE_VPKD).zip"
    endif
  endif
endif

release:
	$(MAKE) VMC=1 GSM=1 IGS=1 CHEAT=1 RELEASE=1 all
	
childproof:
	$(MAKE) CHILDPROOF=1 all

debug:
	$(MAKE) DEBUG=1 all

iopcore_debug:
	$(MAKE) DEBUG=1 IOPCORE_DEBUG=1 all

eesio_debug:
	$(MAKE) DEBUG=1 EESIO_DEBUG=1 all

ingame_debug:
	$(MAKE) DEBUG=1 INGAME_DEBUG=1 all

deci2_debug:
	$(MAKE) DEBUG=1 INGAME_DEBUG=1 DECI2_DEBUG=1 all

clean:
	echo "Cleaning..."
	echo "-Interface"
	rm -fr $(MAPFILE) $(EE_BIN) $(EE_BIN_PKD) $(EE_VPKD).* $(EE_OBJS_DIR) $(EE_ASM_DIR)
	echo "-EE core"
	$(MAKE) -C ee_core clean
	echo "-Elf Loader"
	$(MAKE) -C elfldr clean
	echo "-IOP core"
	echo " -udnl-t300"
	$(MAKE) -C modules/iopcore/udnl-t300 clean
	echo " -udnl"
	$(MAKE) -C modules/iopcore/udnl clean
	echo " -imgdrv"
	$(MAKE) -C modules/iopcore/imgdrv clean
	echo " -eesync"
	$(MAKE) -C modules/iopcore/eesync clean
	echo " -cdvdman"
	$(MAKE) -C modules/iopcore/cdvdman -f Makefile.usb clean
	$(MAKE) -C modules/iopcore/cdvdman -f Makefile.smb clean
	$(MAKE) -C modules/iopcore/cdvdman -f Makefile.hdd clean
	$(MAKE) -C modules/iopcore/cdvdman -f Makefile.hdd.hdpro clean
	echo " -cdvdfsv"
	$(MAKE) -C modules/iopcore/cdvdfsv clean
	echo " -isofs"
	$(MAKE) -C modules/isofs clean
	echo " -usbhdfsd"
	$(MAKE) -C modules/usb/usbhdfsd clean
	echo " -usbhdfsdfsv"
	$(MAKE) -C modules/usb/usbhdfsdfsv clean
	echo " -ps2dev9"
	$(MAKE) -C modules/dev9 clean
	echo " -SMSUTILS"
	$(MAKE) -C modules/network/SMSUTILS clean
	echo " -SMSTCPIP"
	$(MAKE) -C modules/network/SMSTCPIP clean
	echo " -in-game SMAP"
	$(MAKE) -C modules/network/smap-ingame clean
	echo " -smbinit"
	$(MAKE) -C modules/network/smbinit clean
	echo " -nbns"
	$(MAKE) -C modules/network/nbns clean
	echo " -httpclient"
	$(MAKE) -C modules/network/httpclient clean
	echo " -ps2atad"
	$(MAKE) -C modules/hdd/atad clean
	echo " -hdpro_atad"
	$(MAKE) -C modules/hdd/hdpro_atad clean
	echo " -ps2hdd"
	$(MAKE) -C modules/hdd/apa clean
	echo " -ps2fs"
	$(MAKE) -C modules/hdd/pfs clean
	echo " -mcemu"
	$(MAKE) -C modules/mcemu -f Makefile.usb clean
	$(MAKE) -C modules/mcemu -f Makefile.hdd clean
	$(MAKE) -C modules/mcemu -f Makefile.smb clean
	echo " -genvmc"
	$(MAKE) -C modules/vmc/genvmc clean
	echo " -hdldsvr"
	$(MAKE) -C modules/hdd/hdldsvr clean
	echo " -udptty"
	$(MAKE) -C modules/debug/udptty clean
	echo " -udptty-ingame"
	$(MAKE) -C modules/debug/udptty-ingame clean
	echo " -ioptrap"
	$(MAKE) -C modules/debug/ioptrap clean
	echo " -ps2link"
	$(MAKE) -C modules/debug/ps2link clean
	echo "-pc tools"
	$(MAKE) -C pc clean

rebuild: clean all

pc_tools:
	echo "Building iso2opl, opl2iso and genvmc..."
	$(MAKE) _WIN32=0 -C pc

pc_tools_win32:
	echo "Building WIN32 iso2opl, opl2iso and genvmc..."
	$(MAKE) _WIN32=1 -C pc

ee_core.s:
	echo "-EE core"
	$(MAKE) $(PS2LOGO_FLAGS) $(VMC_FLAGS) $(GSM_FLAGS) $(IGS_FLAGS) $(CHEAT_FLAGS) $(EECORE_EXTRA_FLAGS) -C ee_core
	$(BIN2S) ee_core/ee_core.elf asm/ee_core.s eecore_elf

elfldr.s:
	echo "-Elf Loader"
	$(MAKE) -C elfldr
	$(BIN2S) elfldr/elfldr.elf asm/elfldr.s elfldr_elf
	echo "-IOP core"

udnl.s:
ifeq ($(DTL_T10000),1)
	echo " -udnl-t300"
	$(MAKE) -C modules/iopcore/udnl-t300
	$(BIN2S) modules/iopcore/udnl-t300/udnl.irx asm/udnl.s udnl_irx
else
	echo " -udnl"
	$(MAKE) -C modules/iopcore/udnl
	$(BIN2S) modules/iopcore/udnl/udnl.irx asm/udnl.s udnl_irx
endif

imgdrv.s:
	echo " -imgdrv"
	$(MAKE) -C modules/iopcore/imgdrv
	$(BIN2S) modules/iopcore/imgdrv/imgdrv.irx asm/imgdrv.s imgdrv_irx

eesync.s:
	echo " -eesync"
	$(MAKE) -C modules/iopcore/eesync
	$(BIN2S) modules/iopcore/eesync/eesync.irx asm/eesync.s eesync_irx

usb_cdvdman.s:
	echo " -usb_cdvdman"
	$(MAKE) $(VMC_FLAGS) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) -C modules/iopcore/cdvdman -f Makefile.usb rebuild
	$(BIN2S) modules/iopcore/cdvdman/cdvdman.irx asm/usb_cdvdman.s usb_cdvdman_irx

smb_cdvdman.s:
	echo " -smb_cdvdman"
	$(MAKE) $(VMC_FLAGS) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) -C modules/iopcore/cdvdman -f Makefile.smb rebuild
	$(BIN2S) modules/iopcore/cdvdman/cdvdman.irx asm/smb_cdvdman.s smb_cdvdman_irx

hdd_cdvdman.s:
	echo " -hdd_cdvdman"
	$(MAKE) $(VMC_FLAGS) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) -C modules/iopcore/cdvdman -f Makefile.hdd rebuild
	$(BIN2S) modules/iopcore/cdvdman/cdvdman.irx asm/hdd_cdvdman.s hdd_cdvdman_irx

hdd_hdpro_cdvdman.s:
	echo " -hdd_hdpro_cdvdman"
	$(MAKE) $(VMC_FLAGS) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) -C modules/iopcore/cdvdman -f Makefile.hdd.hdpro rebuild
	$(BIN2S) modules/iopcore/cdvdman/cdvdman.irx asm/hdd_hdpro_cdvdman.s hdd_hdpro_cdvdman_irx

cdvdfsv.s:
	echo " -cdvdfsv"
	$(MAKE) -C modules/iopcore/cdvdfsv
	$(BIN2S) modules/iopcore/cdvdfsv/cdvdfsv.irx asm/cdvdfsv.s cdvdfsv_irx

usb_mcemu.s:
	echo " -usb_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) -C modules/mcemu -f Makefile.usb rebuild
	$(BIN2S) modules/mcemu/mcemu.irx asm/usb_mcemu.s usb_mcemu_irx

hdd_mcemu.s:
	echo " -hdd_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) -C modules/mcemu -f Makefile.hdd rebuild
	$(BIN2S) modules/mcemu/mcemu.irx asm/hdd_mcemu.s hdd_mcemu_irx

smb_mcemu.s:
	echo " -smb_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) -C modules/mcemu -f Makefile.smb rebuild
	$(BIN2S) modules/mcemu/mcemu.irx asm/smb_mcemu.s smb_mcemu_irx

isofs.s:
	echo " -isofs"
	$(MAKE) -C modules/isofs
	$(BIN2S) modules/isofs/isofs.irx asm/isofs.s isofs_irx

usbd.s:
	$(BIN2S) $(PS2SDK)/iop/irx/usbd.irx asm/usbd.s usbd_irx

usbhdfsd.s:
	echo " -usbhdfsd"
	$(MAKE) -C modules/usb/usbhdfsd
	$(BIN2S) modules/usb/usbhdfsd/usbhdfsd.irx asm/usbhdfsd.s usbhdfsd_irx

usbhdfsdfsv.s:
	echo " -usbhdfsdfsv"
	$(MAKE) -C modules/usb/usbhdfsdfsv
	$(BIN2S) modules/usb/usbhdfsdfsv/usbhdfsdfsv.irx asm/usbhdfsdfsv.s usbhdfsdfsv_irx

ps2dev9.s:
	echo " -ps2dev9"
	$(MAKE) -C modules/dev9
	$(BIN2S) modules/dev9/ps2dev9.irx asm/ps2dev9.s ps2dev9_irx

smsutils.s:
	echo " -SMSUTILS"
	$(MAKE) -C modules/network/SMSUTILS
	$(BIN2S) modules/network/SMSUTILS/SMSUTILS.irx asm/smsutils.s smsutils_irx

ps2ip.s:
	$(BIN2S) $(PS2SDK)/iop/irx/ps2ip-nm.irx asm/ps2ip.s ps2ip_irx

ingame_smstcpip.s:
	echo " -in-game SMSTCPIP"
	$(MAKE) $(SMSTCPIP_INGAME_CFLAGS) -C modules/network/SMSTCPIP rebuild
	$(BIN2S) modules/network/SMSTCPIP/SMSTCPIP.irx asm/ingame_smstcpip.s ingame_smstcpip_irx

smap_ingame.s:
	echo " -in-game SMAP"
	$(MAKE) -C modules/network/smap-ingame
	$(BIN2S) modules/network/smap-ingame/smap.irx asm/smap_ingame.s smap_ingame_irx

smap.s:
	$(BIN2S) $(PS2SDK)/iop/irx/smap.irx asm/smap.s smap_irx

netman.s:
	$(BIN2S) $(PS2SDK)/iop/irx/netman.irx asm/netman.s netman_irx

ps2ips.s:
	$(BIN2S) $(PS2SDK)/iop/irx/ps2ips.irx asm/ps2ips.s ps2ips_irx

smbman.s:
	$(BIN2S) $(PS2SDK)/iop/irx/smbman.irx asm/smbman.s smbman_irx

smbinit.s:
	echo " -smbinit"
	$(MAKE) -C modules/network/smbinit
	$(BIN2S) modules/network/smbinit/smbinit.irx asm/smbinit.s smbinit_irx

ps2atad.s:
	echo " -ps2atad"
	$(MAKE) -C modules/hdd/atad
	$(BIN2S) modules/hdd/atad/ps2atad.irx asm/ps2atad.s ps2atad_irx

hdpro_atad.s:
	echo " -hdpro_atad"
	$(MAKE) -C modules/hdd/hdpro_atad
	$(BIN2S) modules/hdd/hdpro_atad/hdpro_atad.irx asm/hdpro_atad.s hdpro_atad_irx

poweroff.s:
	$(BIN2S) $(PS2SDK)/iop/irx/poweroff.irx asm/poweroff.s poweroff_irx

ps2hdd.s:
	echo " -ps2hdd"
	$(MAKE) -C modules/hdd/apa
	$(BIN2S) modules/hdd/apa/ps2hdd.irx asm/ps2hdd.s ps2hdd_irx

genvmc.s:
	echo " -genvmc"
	$(MAKE) $(MOD_DEBUG_FLAGS) -C modules/vmc/genvmc
	$(BIN2S) modules/vmc/genvmc/genvmc.irx asm/genvmc.s genvmc_irx

hdldsvr.s:
	echo " -hdldsvr"
	$(MAKE) -C modules/hdd/hdldsvr
	$(BIN2S) modules/hdd/hdldsvr/hdldsvr.irx asm/hdldsvr.s hdldsvr_irx

udptty.s:
	echo " -udptty"
	$(MAKE) -C modules/debug/udptty
	$(BIN2S) modules/debug/udptty/udptty.irx asm/udptty.s udptty_irx

udptty-ingame.s:
	echo " -udptty-ingame"
	$(MAKE) -C modules/debug/udptty-ingame
	$(BIN2S) modules/debug/udptty-ingame/udptty.irx asm/udptty-ingame.s udptty_ingame_irx

ioptrap.s:
	echo " -ioptrap"
	$(MAKE) -C modules/debug/ioptrap
	$(BIN2S) modules/debug/ioptrap/ioptrap.irx asm/ioptrap.s ioptrap_irx

ps2link.s:
	echo " -ps2link"
	$(MAKE) -C modules/debug/ps2link
	$(BIN2S) modules/debug/ps2link/ps2link.irx asm/ps2link.s ps2link_irx

nbns-iop.s:
	echo " -nbns"
	$(MAKE) -C modules/network/nbns
	$(BIN2S) modules/network/nbns/nbns.irx asm/nbns-iop.s nbns_irx

httpclient-iop.s:
	echo " -httpclient"
	$(MAKE) -C modules/network/httpclient
	$(BIN2S) modules/network/httpclient/httpclient.irx asm/httpclient-iop.s httpclient_irx
	
ps2fs.s:
	echo " -ps2fs"
	$(MAKE) -C modules/hdd/pfs
	$(BIN2S) modules/hdd/pfs/ps2fs.irx asm/ps2fs.s ps2fs_irx

iomanx.s:
	$(BIN2S) $(PS2SDK)/iop/irx/iomanX.irx asm/iomanx.s iomanx_irx

filexio.s:
	$(BIN2S) $(PS2SDK)/iop/irx/fileXio.irx asm/filexio.s filexio_irx

sio2man.s:
	$(BIN2S) $(PS2SDK)/iop/irx/freesio2.irx asm/sio2man.s sio2man_irx

padman.s:
	$(BIN2S) $(PS2SDK)/iop/irx/freepad.irx asm/padman.s padman_irx

mcman.s:
	$(BIN2S) $(PS2SDK)/iop/irx/mcman.irx asm/mcman.s mcman_irx

mcserv.s:
	$(BIN2S) $(PS2SDK)/iop/irx/mcserv.irx asm/mcserv.s mcserv_irx

load0.s:
	$(BIN2S) gfx/load0.png asm/load0.s load0_png

load1.s:
	$(BIN2S) gfx/load1.png asm/load1.s load1_png

load2.s:
	$(BIN2S) gfx/load2.png asm/load2.s load2_png

load3.s:
	$(BIN2S) gfx/load3.png asm/load3.s load3_png

load4.s:
	$(BIN2S) gfx/load4.png asm/load4.s load4_png

load5.s:
	$(BIN2S) gfx/load5.png asm/load5.s load5_png

load6.s:
	$(BIN2S) gfx/load6.png asm/load6.s load6_png

load7.s:
	$(BIN2S) gfx/load7.png asm/load7.s load7_png

logo.s:
	$(BIN2S) gfx/logo.png asm/logo.s logo_png

bg_overlay.s:
	$(BIN2S) gfx/bg_overlay.png asm/bg_overlay.s bg_overlay_png

usb_icon.s:
	$(BIN2S) gfx/usb.png asm/usb_icon.s usb_png

hdd_icon.s:
	$(BIN2S) gfx/hdd.png asm/hdd_icon.s hdd_png

eth_icon.s:
	$(BIN2S) gfx/eth.png asm/eth_icon.s eth_png

app_icon.s:
	$(BIN2S) gfx/app.png asm/app_icon.s app_png

cross_icon.s:
	$(BIN2S) gfx/cross.png asm/cross_icon.s cross_png

triangle_icon.s:
	$(BIN2S) gfx/triangle.png asm/triangle_icon.s triangle_png

circle_icon.s:
	$(BIN2S) gfx/circle.png asm/circle_icon.s circle_png

square_icon.s:
	$(BIN2S) gfx/square.png asm/square_icon.s square_png

select_icon.s:
	$(BIN2S) gfx/select.png asm/select_icon.s select_png

start_icon.s:
	$(BIN2S) gfx/start.png asm/start_icon.s start_png

left_icon.s:
	$(BIN2S) gfx/left.png asm/left_icon.s left_png

right_icon.s:
	$(BIN2S) gfx/right.png asm/right_icon.s right_png

up_icon.s:
	$(BIN2S) gfx/up.png asm/up_icon.s up_png

down_icon.s:
	$(BIN2S) gfx/down.png asm/down_icon.s down_png

L1_icon.s:
	$(BIN2S) gfx/L1.png asm/L1_icon.s L1_png

L2_icon.s:
	$(BIN2S) gfx/L2.png asm/L2_icon.s L2_png

R1_icon.s:
	$(BIN2S) gfx/R1.png asm/R1_icon.s R1_png

R2_icon.s:
	$(BIN2S) gfx/R2.png asm/R2_icon.s R2_png

freesans.s:
	$(BIN2S) thirdparty/FreeSans_basic_latin.ttf asm/freesans.s freesansfont_raw

icon_sys.s:
	$(BIN2S) gfx/icon.sys asm/icon_sys.s icon_sys
	
icon_icn.s:
	$(BIN2S) gfx/opl.icn asm/icon_icn.s icon_icn	

icon_sys_A.s:
	$(BIN2S) misc/icon_A.sys asm/icon_sys_A.s icon_sys_A

icon_sys_J.s:
	$(BIN2S) misc/icon_J.sys asm/icon_sys_J.s icon_sys_J

IOPRP_img.s:
	$(BIN2S) modules/iopcore/IOPRP.img asm/IOPRP_img.s IOPRP_img

drvtif_irx.s:
	$(BIN2S) modules/debug/drvtif.irx asm/drvtif_irx.s drvtif_irx

tifinet_irx.s:
	$(BIN2S) modules/debug/tifinet.irx asm/tifinet_irx.s tifinet_irx

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $(EE_ASM_DIR)$< -o $@

oplversion:
	@echo $(OPLVERSION)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
