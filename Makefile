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

# I want to put my name in my custom build! How can I do it?
# Type "make LOCALVERSION=-foobar"

# ======== START OF CONFIGURABLE SECTION ========
# You can adjust the variables in this section to meet your needs.
# To enable a feature, set its variable's value to 1. To disable, change it to 0.
# Do not COMMENT out the variables!!
# You can also specify variables when executing make: "make RTL=1 IGS=1 PADEMU=1"

#Enables/disables Right-To-Left (RTL) language support
RTL ?= 0

#Enables/disables In Game Screenshot (IGS). NB: It depends on GSM and IGR to work
IGS ?= 0

#Enables/disables pad emulator
PADEMU ?= 0

#Enables/disables building of an edition of OPL that will support the DTL-T10000 (SDK v2.3+)
DTL_T10000 ?= 0

#Nor stripping neither compressing binary ELF after compiling.
NOT_PACKED ?= 0

# ======== END OF CONFIGURABLE SECTION. DO NOT MODIFY VARIABLES AFTER THIS POINT!! ========
DEBUG ?= 0
EESIO_DEBUG ?= 0
INGAME_DEBUG ?= 0
DECI2_DEBUG ?= 0

# ======== DO NOT MODIFY VALUES AFTER THIS POINT! UNLESS YOU KNOW WHAT YOU ARE DOING ========
REVISION = $(shell expr $(shell git rev-list --count HEAD) + 2)

GIT_HASH = $(shell git rev-parse --short=7 HEAD 2>/dev/null)
ifeq ($(shell git diff --quiet; echo $$?),1)
  DIRTY = -dirty
endif
ifneq ($(shell test -d .git; echo $$?),0)
  DIRTY = -dirty
endif

OPL_VERSION = $(VERSION).$(SUBVERSION).$(PATCHLEVEL).$(REVISION)$(if $(EXTRAVERSION),-$(EXTRAVERSION))$(if $(GIT_HASH),-$(GIT_HASH))$(if $(DIRTY),$(DIRTY))$(if $(LOCALVERSION),-$(LOCALVERSION))

FRONTEND_OBJS = pad.o fntsys.o renderman.o menusys.o OSDHistory.o system.o lang.o config.o hdd.o dialogs.o \
		dia.o ioman.o texcache.o themes.o supportbase.o usbsupport.o ethsupport.o hddsupport.o \
		appsupport.o gui.o textures.o opl.o atlas.o nbns.o httpclient.o gsm.o cheatman.o sound.o ps2cnf.o

GFX_OBJS =	usb_icon.o hdd_icon.o eth_icon.o app_icon.o \
		cross_icon.o triangle_icon.o circle_icon.o square_icon.o select_icon.o start_icon.o \
		left_icon.o right_icon.o up_icon.o down_icon.o L1_icon.o L2_icon.o R1_icon.o R2_icon.o \
		load0.o load1.o load2.o load3.o load4.o load5.o load6.o load7.o logo.o bg_overlay.o freesans.o \
		icon_sys.o icon_icn.o

MISC_OBJS =	icon_sys_A.o icon_sys_J.o icon_sys_C.o conf_theme_OPL.o \
		boot.o cancel.o confirm.o cursor.o message.o transition.o

IOP_OBJS =	iomanx.o filexio.o ps2fs.o usbd.o usbhdfsd.o usbhdfsdfsv.o \
		ps2atad.o hdpro_atad.o poweroff.o ps2hdd.o xhdd.o genvmc.o hdldsvr.o \
		ps2dev9.o smsutils.o ps2ip.o smap.o isofs.o nbns-iop.o \
		httpclient-iop.o netman.o ps2ips.o \
		usb_mcemu.o hdd_mcemu.o smb_mcemu.o \
		iremsndpatch.o apemodpatch.o f2techioppatch.o cleareffects.o resetspu.o \
		libsd.o audsrv.o

EECORE_OBJS = ee_core.o ioprp.o util.o \
		elfldr.o udnl.o imgdrv.o eesync.o \
		usb_cdvdman.o IOPRP_img.o smb_cdvdman.o \
		hdd_cdvdman.o hdd_hdpro_cdvdman.o cdvdfsv.o \
		ingame_smstcpip.o smap_ingame.o smbman.o smbinit.o

EE_BIN = opl.elf
EE_BIN_STRIPPED = opl_stripped.elf
EE_BIN_PACKED = OPNPS2LD.ELF
EE_VPKD = OPNPS2LD-$(OPL_VERSION)
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/

MAPFILE = opl.map
EE_LDFLAGS += -Wl,-Map,$(MAPFILE)

EE_LIBS = -L$(PS2SDK)/ports/lib -L$(GSKIT)/lib -L./lib -lgskit -ldmakit -lgskit_toolkit -lpoweroff -lfileXio -lpatches -ljpeg -lpng -lz -ldebug -lm -lmc -lfreetype -lvux -lcdvd -lnetman -lps2ips -laudsrv -lc
EE_INCS += -I$(PS2SDK)/ports/include -I$(GSKIT)/include -I$(GSKIT)/ee/dma/include -I$(GSKIT)/ee/gs/include -I$(GSKIT)/ee/toolkit/include -Imodules/iopcore/common -Imodules/network/common -Imodules/hdd/common -Iinclude

BIN2C = $(PS2SDK)/bin/bin2c
BIN2S = $(PS2SDK)/bin/bin2s
BIN2O = $(PS2SDK)/bin/bin2o

# WARNING: Only extra spaces are allowed and ignored at the beginning of the conditional directives (ifeq, ifneq, ifdef, ifndef, else and endif)
# but a tab is not allowed; if the line begins with a tab, it will be considered part of a recipe for a rule!

ifeq ($(RTL),1)
  EE_CFLAGS += -D__RTL
endif

ifeq ($(DTL_T10000),1)
  EE_CFLAGS += -D_DTL_T10000
  EECORE_EXTRA_FLAGS += DTL_T10000=1
  IOP_OBJS += sio2man.o padman.o mcman.o mcserv.o
  EE_LIBS += -lpadx
  UDNL_SRC = modules/iopcore/udnl-t300
  UDNL_OUT = modules/iopcore/udnl-t300/udnl.irx
else
  EE_LIBS += -lpad
  UDNL_SRC = modules/iopcore/udnl
  UDNL_OUT = modules/iopcore/udnl/udnl.irx
endif

ifeq ($(IGS),1)
  EE_CFLAGS += -DIGS
  IGS_FLAGS = IGS=1
else
  IGS_FLAGS = IGS=0
endif

ifeq ($(PADEMU),1)
  IOP_OBJS += bt_pademu.o usb_pademu.o ds34usb.o ds34bt.o libds34usb.a libds34bt.a
  EE_CFLAGS += -DPADEMU
  EE_INCS += -Imodules/ds34bt/ee -Imodules/ds34usb/ee
  PADEMU_FLAGS = PADEMU=1
else
  PADEMU_FLAGS = PADEMU=0
endif

ifeq ($(DEBUG),1)
  EE_CFLAGS += -D__DEBUG -g
  ifeq ($(DECI2_DEBUG),1)
    EE_OBJS += debug.o drvtif_irx.o tifinet_irx.o deci2_img.o
    EE_LDFLAGS += -liopreboot
  else
    EE_OBJS += debug.o udptty.o ioptrap.o ps2link.o
  endif
  MOD_DEBUG_FLAGS = DEBUG=1
  ifeq ($(IOPCORE_DEBUG),1)
    EE_CFLAGS += -D__INGAME_DEBUG
    EECORE_EXTRA_FLAGS = LOAD_DEBUG_MODULES=1
    CDVDMAN_DEBUG_FLAGS = IOPCORE_DEBUG=1
    MCEMU_DEBUG_FLAGS = IOPCORE_DEBUG=1
    SMSTCPIP_INGAME_CFLAGS =
    IOP_OBJS += udptty-ingame.o
  else ifeq ($(EESIO_DEBUG),1)
    EE_CFLAGS += -D__EESIO_DEBUG
    EECORE_EXTRA_FLAGS += EESIO_DEBUG=1
  else ifeq ($(INGAME_DEBUG),1)
    EE_CFLAGS += -D__INGAME_DEBUG
    EECORE_EXTRA_FLAGS = LOAD_DEBUG_MODULES=1
    CDVDMAN_DEBUG_FLAGS = IOPCORE_DEBUG=1
    SMSTCPIP_INGAME_CFLAGS =
    ifeq ($(DECI2_DEBUG),1)
      EE_CFLAGS += -D__DECI2_DEBUG
      EECORE_EXTRA_FLAGS += DECI2_DEBUG=1
      IOP_OBJS += drvtif_ingame_irx.o tifinet_ingame_irx.o
      DECI2_DEBUG=1
      CDVDMAN_DEBUG_FLAGS = USE_DEV9=1 #(clear IOPCORE_DEBUG) dsidb cannot be used to handle exceptions or set breakpoints, so disable output to save resources.
    else
      IOP_OBJS += udptty-ingame.o
    endif
  endif
else
  EE_CFLAGS += -O2
  SMSTCPIP_INGAME_CFLAGS = INGAME_DRIVER=1
endif

EE_CFLAGS += -DOPL_VERSION=\"$(OPL_VERSION)\"
EE_OBJS += $(FRONTEND_OBJS) $(GFX_OBJS) $(MISC_OBJS) $(EECORE_OBJS) $(IOP_OBJS)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.SILENT:

.PHONY: all release debug iopcore_debug eesio_debug ingame_debug deci2_debug clean rebuild pc_tools pc_tools_win32 oplversion

all:
	echo "Building Open PS2 Loader $(OPL_VERSION)..."
	echo "-Interface"
ifneq ($(NOT_PACKED),1)
	$(MAKE) $(EE_BIN_PACKED)
else
	$(MAKE) $(EE_BIN)
endif

release:
	echo "Building Open PS2 Loader $(OPL_VERSION)..."
	echo "-Interface"
	$(MAKE) IGS=1 PADEMU=1 $(EE_VPKD).ZIP

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
	rm -fr $(MAPFILE) $(EE_BIN) $(EE_BIN_PACKED) $(EE_BIN_STRIPPED) $(EE_VPKD).* $(EE_OBJS_DIR) $(EE_ASM_DIR)
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
	$(MAKE) -C modules/iopcore/cdvdman USE_USB=1 clean
	$(MAKE) -C modules/iopcore/cdvdman USE_SMB=1 clean
	$(MAKE) -C modules/iopcore/cdvdman USE_HDD=1 clean
	$(MAKE) -C modules/iopcore/cdvdman USE_HDPRO=1 clean
	echo " -cdvdfsv"
	$(MAKE) -C modules/iopcore/cdvdfsv clean
	echo " -resetspu"
	$(MAKE) -C modules/iopcore/resetspu clean
	echo "  -patches"
	echo "   -iremsnd"
	$(MAKE) -C modules/iopcore/patches/iremsndpatch clean
	echo "   -apemod"
	$(MAKE) -C modules/iopcore/patches/apemodpatch clean
	echo "   -f2techiop"
	$(MAKE) -C modules/iopcore/patches/f2techioppatch clean
	echo "   -cleareffects"
	$(MAKE) -C modules/iopcore/patches/cleareffects clean
	echo " -isofs"
	$(MAKE) -C modules/isofs clean
	echo " -usbhdfsdfsv"
	$(MAKE) -C modules/usb/usbhdfsdfsv clean
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
	echo " -xhdd"
	$(MAKE) -C modules/hdd/xhdd clean
	echo " -mcemu"
	$(MAKE) -C modules/mcemu USE_USB=1 clean
	$(MAKE) -C modules/mcemu USE_HDD=1 clean
	$(MAKE) -C modules/mcemu USE_SMB=1 clean
	echo " -genvmc"
	$(MAKE) -C modules/vmc/genvmc clean
	echo " -hdldsvr"
	$(MAKE) -C modules/hdd/hdldsvr clean
	echo " -udptty-ingame"
	$(MAKE) -C modules/debug/udptty-ingame clean
	echo " -ioptrap"
	$(MAKE) -C modules/debug/ioptrap clean
	echo " -ps2link"
	$(MAKE) -C modules/debug/ps2link clean
	echo " -ds34usb"
	$(MAKE) -C modules/ds34usb clean
	echo " -ds34bt"
	$(MAKE) -C modules/ds34bt clean
	echo " -pademu"
	$(MAKE) -C modules/pademu USE_BT=1 clean
	$(MAKE) -C modules/pademu USE_USB=1 clean
	echo "-pc tools"
	$(MAKE) -C pc clean

rebuild: clean all

pc_tools:
	echo "Building iso2opl, opl2iso and genvmc..."
	$(MAKE) _WIN32=0 -C pc

pc_tools_win32:
	echo "Building WIN32 iso2opl, opl2iso and genvmc..."
	$(MAKE) _WIN32=1 -C pc

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

.PHONY: DETAILED_CHANGELOG
DETAILED_CHANGELOG:
	sh make_changelog.sh

$(EE_BIN_STRIPPED): $(EE_BIN)
	echo "Stripping..."
	$(EE_STRIP) -o $@ $<

$(EE_BIN_PACKED): $(EE_BIN_STRIPPED)
	echo "Compressing..."
	ps2-packer $< $@ > /dev/null

$(EE_VPKD).ELF: $(EE_BIN_PACKED)
	cp -f $< $@

$(EE_VPKD).ZIP: $(EE_VPKD).ELF DETAILED_CHANGELOG CREDITS LICENSE README.md
	zip -r $@ $^
	echo "Package Complete: $@"

ee_core/ee_core.elf: ee_core
	echo "-EE core"
	$(MAKE) $(IGS_FLAGS) $(PADEMU_FLAGS) $(EECORE_EXTRA_FLAGS) -C $<

$(EE_ASM_DIR)ee_core.s: ee_core/ee_core.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ eecore_elf

elfldr/elfldr.elf: elfldr
	echo "-Elf Loader"
	$(MAKE) -C $<

$(EE_ASM_DIR)elfldr.s: elfldr/elfldr.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ elfldr_elf

$(UDNL_OUT): $(UDNL_SRC)
	echo "-IOP core"
	echo " -udnl"
	$(MAKE) -C $<

$(EE_ASM_DIR)udnl.s: $(UDNL_OUT) | $(EE_ASM_DIR)
	$(BIN2S) $(UDNL_OUT) $@ udnl_irx

modules/iopcore/imgdrv/imgdrv.irx: modules/iopcore/imgdrv
	echo " -imgdrv"
	$(MAKE) -C $<

$(EE_ASM_DIR)imgdrv.s: modules/iopcore/imgdrv/imgdrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ imgdrv_irx

modules/iopcore/eesync/eesync.irx: modules/iopcore/eesync
	echo " -eesync"
	$(MAKE) -C $<

$(EE_ASM_DIR)eesync.s: modules/iopcore/eesync/eesync.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ eesync_irx

modules/iopcore/cdvdman/usb_cdvdman.irx: modules/iopcore/cdvdman
	echo " -usb_cdvdman"
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_USB=1 -C $< rebuild

$(EE_ASM_DIR)usb_cdvdman.s: modules/iopcore/cdvdman/usb_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usb_cdvdman_irx

modules/iopcore/cdvdman/smb_cdvdman.irx: modules/iopcore/cdvdman
	echo " -smb_cdvdman"
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_SMB=1 -C $< rebuild

$(EE_ASM_DIR)smb_cdvdman.s: modules/iopcore/cdvdman/smb_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smb_cdvdman_irx

modules/iopcore/cdvdman/hdd_cdvdman.irx: modules/iopcore/cdvdman
	echo " -hdd_cdvdman"
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_HDD=1 -C $< rebuild

$(EE_ASM_DIR)hdd_cdvdman.s: modules/iopcore/cdvdman/hdd_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdd_cdvdman_irx

modules/iopcore/cdvdman/hdd_hdpro_cdvdman.irx: modules/iopcore/cdvdman
	echo " -hdd_hdpro_cdvdman"
	$(MAKE) $(CDVDMAN_PS2LOGO_FLAGS) $(CDVDMAN_DEBUG_FLAGS) USE_HDPRO=1 -C $< rebuild

$(EE_ASM_DIR)hdd_hdpro_cdvdman.s: modules/iopcore/cdvdman/hdd_hdpro_cdvdman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdd_hdpro_cdvdman_irx

modules/iopcore/cdvdfsv/cdvdfsv.irx: modules/iopcore/cdvdfsv
	echo " -cdvdfsv"
	$(MAKE) -C $<

$(EE_ASM_DIR)cdvdfsv.s: modules/iopcore/cdvdfsv/cdvdfsv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdvdfsv_irx

modules/iopcore/patches/iremsndpatch/iremsndpatch.irx: modules/iopcore/patches/iremsndpatch
	echo " -iremsnd patch"
	$(MAKE) -C $<

$(EE_ASM_DIR)iremsndpatch.s: modules/iopcore/patches/iremsndpatch/iremsndpatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iremsndpatch_irx

modules/iopcore/patches/apemodpatch/apemodpatch.irx: modules/iopcore/patches/apemodpatch
	echo " -apemod patch"
	$(MAKE) -C $<

$(EE_ASM_DIR)apemodpatch.s: modules/iopcore/patches/apemodpatch/apemodpatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ apemodpatch_irx

modules/iopcore/patches/f2techioppatch/f2techioppatch.irx: modules/iopcore/patches/f2techioppatch
	echo " -f2techiop patch"
	$(MAKE) -C $<

$(EE_ASM_DIR)f2techioppatch.s: modules/iopcore/patches/f2techioppatch/f2techioppatch.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ f2techioppatch_irx

modules/iopcore/patches/cleareffects/cleareffects.irx: modules/iopcore/patches/cleareffects
	echo " -cleareffects"
	$(MAKE) -C $<

$(EE_ASM_DIR)cleareffects.s: modules/iopcore/patches/cleareffects/cleareffects.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cleareffects_irx

modules/iopcore/resetspu/resetspu.irx: modules/iopcore/resetspu
	echo " -resetspu"
	$(MAKE) -C $<

$(EE_ASM_DIR)resetspu.s: modules/iopcore/resetspu/resetspu.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ resetspu_irx

modules/mcemu/usb_mcemu.irx: modules/mcemu
	echo " -usb_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_USB=1 -C $< rebuild

$(EE_ASM_DIR)usb_mcemu.s: modules/mcemu/usb_mcemu.irx
	$(BIN2S) $< $@ usb_mcemu_irx

modules/mcemu/hdd_mcemu.irx: modules/mcemu
	echo " -hdd_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_HDD=1 -C $< rebuild

$(EE_ASM_DIR)hdd_mcemu.s: modules/mcemu/hdd_mcemu.irx
	$(BIN2S) $< $@ hdd_mcemu_irx

modules/mcemu/smb_mcemu.irx: modules/mcemu
	echo " -smb_mcemu"
	$(MAKE) $(MCEMU_DEBUG_FLAGS) $(PADEMU_FLAGS) USE_SMB=1 -C $< rebuild

$(EE_ASM_DIR)smb_mcemu.s: modules/mcemu/smb_mcemu.irx
	$(BIN2S) $< $@ smb_mcemu_irx

modules/isofs/isofs.irx: modules/isofs
	echo " -isofs"
	$(MAKE) -C $<

$(EE_ASM_DIR)isofs.s: modules/isofs/isofs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ isofs_irx

$(EE_ASM_DIR)usbd.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx

$(EE_ASM_DIR)libsd.s: $(PS2SDK)/iop/irx/libsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ libsd_irx

$(EE_ASM_DIR)audsrv.s: $(PS2SDK)/iop/irx/audsrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ audsrv_irx

$(EE_OBJS_DIR)libds34bt.a: modules/ds34bt/ee/libds34bt.a
	cp $< $@

modules/ds34bt/ee/libds34bt.a: modules/ds34bt/ee
	$(MAKE) -C $<

modules/ds34bt/iop/ds34bt.irx: modules/ds34bt/iop
	echo " -ds34bt"
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34bt.s: modules/ds34bt/iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

$(EE_OBJS_DIR)libds34usb.a: modules/ds34usb/ee/libds34usb.a
	cp $< $@

modules/ds34usb/ee/libds34usb.a: modules/ds34usb/ee
	$(MAKE) -C $<

modules/ds34usb/iop/ds34usb.irx: modules/ds34usb/iop
	echo " -ds34usb"
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: modules/ds34usb/iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

modules/pademu/bt_pademu.irx: modules/pademu
	echo " -bt_pademu"
	$(MAKE) -C $< USE_BT=1

$(EE_ASM_DIR)bt_pademu.s: modules/pademu/bt_pademu.irx
	$(BIN2S) $< $@ bt_pademu_irx

modules/pademu/usb_pademu.irx: modules/pademu
	echo " -usb_pademu"
	$(MAKE) -C $< USE_USB=1

$(EE_ASM_DIR)usb_pademu.s: modules/pademu/usb_pademu.irx
	$(BIN2S) $< $@ usb_pademu_irx

$(EE_ASM_DIR)usbhdfsd.s: $(PS2SDK)/iop/irx/usbhdfsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbhdfsd_irx

modules/usb/usbhdfsdfsv/usbhdfsdfsv.irx: modules/usb/usbhdfsdfsv
	echo " -usbhdfsdfsv"
	$(MAKE) -C $<

$(EE_ASM_DIR)usbhdfsdfsv.s: modules/usb/usbhdfsdfsv/usbhdfsdfsv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbhdfsdfsv_irx

$(EE_ASM_DIR)ps2dev9.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

modules/network/SMSUTILS/SMSUTILS.irx: modules/network/SMSUTILS
	echo " -SMSUTILS"
	$(MAKE) -C $<

$(EE_ASM_DIR)smsutils.s: modules/network/SMSUTILS/SMSUTILS.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smsutils_irx

$(EE_ASM_DIR)ps2ip.s: $(PS2SDK)/iop/irx/ps2ip-nm.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ip_irx

modules/network/SMSTCPIP/SMSTCPIP.irx: modules/network/SMSTCPIP
	echo " -in-game SMSTCPIP"
	$(MAKE) $(SMSTCPIP_INGAME_CFLAGS) -C $< rebuild

$(EE_ASM_DIR)ingame_smstcpip.s: modules/network/SMSTCPIP/SMSTCPIP.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ingame_smstcpip_irx

modules/network/smap-ingame/smap.irx: modules/network/smap-ingame
	echo " -in-game SMAP"
	$(MAKE) -C $<

$(EE_ASM_DIR)smap_ingame.s: modules/network/smap-ingame/smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smap_ingame_irx

$(EE_ASM_DIR)smap.s: $(PS2SDK)/iop/irx/smap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smap_irx

$(EE_ASM_DIR)netman.s: $(PS2SDK)/iop/irx/netman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ netman_irx

$(EE_ASM_DIR)ps2ips.s: $(PS2SDK)/iop/irx/ps2ips.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ips_irx

$(EE_ASM_DIR)smbman.s: $(PS2SDK)/iop/irx/smbman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smbman_irx

modules/network/smbinit/smbinit.irx: modules/network/smbinit
	echo " -smbinit"
	$(MAKE) -C $<

$(EE_ASM_DIR)smbinit.s: modules/network/smbinit/smbinit.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ smbinit_irx

$(EE_ASM_DIR)ps2atad.s: $(PS2SDK)/iop/irx/ps2atad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2atad_irx

$(EE_ASM_DIR)hdpro_atad.s: $(PS2SDK)/iop/irx/hdproatad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdpro_atad_irx

$(EE_ASM_DIR)poweroff.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

modules/hdd/xhdd/xhdd.irx: modules/hdd/xhdd
	echo " -xhdd"
	$(MAKE) -C $<

$(EE_ASM_DIR)xhdd.s: modules/hdd/xhdd/xhdd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xhdd_irx

$(EE_ASM_DIR)ps2hdd.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs.s: $(PS2SDK)/iop/irx/ps2fs-osd.irx
	$(BIN2S) $< $@ ps2fs_irx

modules/vmc/genvmc/genvmc.irx: modules/vmc/genvmc
	echo " -genvmc"
	$(MAKE) $(MOD_DEBUG_FLAGS) -C $<

$(EE_ASM_DIR)genvmc.s: modules/vmc/genvmc/genvmc.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ genvmc_irx

modules/hdd/hdldsvr/hdldsvr.irx: modules/hdd/hdldsvr
	echo " -hdldsvr"
	$(MAKE) -C $<

$(EE_ASM_DIR)hdldsvr.s: modules/hdd/hdldsvr/hdldsvr.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdldsvr_irx

$(EE_ASM_DIR)udptty.s: $(PS2SDK)/iop/irx/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_irx

modules/debug/udptty-ingame/udptty.irx: modules/debug/udptty-ingame
	echo " -udptty-ingame"
	$(MAKE) -C $<

$(EE_ASM_DIR)udptty-ingame.s: modules/debug/udptty-ingame/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_ingame_irx

modules/debug/ioptrap/ioptrap.irx: modules/debug/ioptrap
	echo " -ioptrap"
	$(MAKE) -C $<

$(EE_ASM_DIR)ioptrap.s: modules/debug/ioptrap/ioptrap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ioptrap_irx

modules/debug/ps2link/ps2link.irx: modules/debug/ps2link
	echo " -ps2link"
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2link.s: modules/debug/ps2link/ps2link.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2link_irx

modules/network/nbns/nbns.irx: modules/network/nbns
	echo " -nbns"
	$(MAKE) -C $<

$(EE_ASM_DIR)nbns-iop.s: modules/network/nbns/nbns.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ nbns_irx

modules/network/httpclient/httpclient.irx: modules/network/httpclient
	echo " -httpclient"
	$(MAKE) -C $<

$(EE_ASM_DIR)httpclient-iop.s: modules/network/httpclient/httpclient.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ httpclient_irx

$(EE_ASM_DIR)iomanx.s: $(PS2SDK)/iop/irx/iomanX.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio.s: $(PS2SDK)/iop/irx/fileXio.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ filexio_irx

$(EE_ASM_DIR)sio2man.s: $(PS2SDK)/iop/irx/freesio2.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

$(EE_ASM_DIR)padman.s: $(PS2SDK)/iop/irx/freepad.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

$(EE_ASM_DIR)mcman.s: $(PS2SDK)/iop/irx/mcman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv.s: $(PS2SDK)/iop/irx/mcserv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)load0.s: gfx/load0.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load0_png

$(EE_ASM_DIR)load1.s: gfx/load1.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load1_png

$(EE_ASM_DIR)load2.s: gfx/load2.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load2_png

$(EE_ASM_DIR)load3.s: gfx/load3.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load3_png

$(EE_ASM_DIR)load4.s: gfx/load4.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load4_png

$(EE_ASM_DIR)load5.s: gfx/load5.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load5_png

$(EE_ASM_DIR)load6.s: gfx/load6.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load6_png

$(EE_ASM_DIR)load7.s: gfx/load7.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ load7_png

$(EE_ASM_DIR)logo.s: gfx/logo.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ logo_png

$(EE_ASM_DIR)bg_overlay.s: gfx/bg_overlay.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bg_overlay_png

$(EE_ASM_DIR)usb_icon.s: gfx/usb.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usb_png

$(EE_ASM_DIR)hdd_icon.s: gfx/hdd.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdd_png

$(EE_ASM_DIR)eth_icon.s: gfx/eth.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ eth_png

$(EE_ASM_DIR)app_icon.s: gfx/app.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ app_png

$(EE_ASM_DIR)cross_icon.s: gfx/cross.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cross_png

$(EE_ASM_DIR)triangle_icon.s: gfx/triangle.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ triangle_png

$(EE_ASM_DIR)circle_icon.s: gfx/circle.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ circle_png

$(EE_ASM_DIR)square_icon.s: gfx/square.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ square_png

$(EE_ASM_DIR)select_icon.s: gfx/select.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ select_png

$(EE_ASM_DIR)start_icon.s: gfx/start.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ start_png

$(EE_ASM_DIR)left_icon.s: gfx/left.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ left_png

$(EE_ASM_DIR)right_icon.s: gfx/right.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ right_png

$(EE_ASM_DIR)up_icon.s: gfx/up.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ up_png

$(EE_ASM_DIR)down_icon.s: gfx/down.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ down_png

$(EE_ASM_DIR)L1_icon.s: gfx/L1.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ L1_png

$(EE_ASM_DIR)L2_icon.s: gfx/L2.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ L2_png

$(EE_ASM_DIR)R1_icon.s: gfx/R1.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ R1_png

$(EE_ASM_DIR)R2_icon.s: gfx/R2.png | $(EE_ASM_DIR)
	$(BIN2S) $< $@ R2_png

$(EE_ASM_DIR)freesans.s: thirdparty/FreeSans_basic_latin.ttf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ freesansfont_raw

$(EE_ASM_DIR)icon_sys.s: gfx/icon.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys

$(EE_ASM_DIR)icon_icn.s: gfx/opl.icn | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_icn

$(EE_ASM_DIR)icon_sys_A.s: misc/icon_A.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_A

$(EE_ASM_DIR)icon_sys_J.s: misc/icon_J.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_J

$(EE_ASM_DIR)icon_sys_C.s: misc/icon_C.sys | $(EE_ASM_DIR)
	$(BIN2S) $< $@ icon_sys_C

$(EE_ASM_DIR)conf_theme_OPL.s: misc/conf_theme_OPL.cfg | $(EE_ASM_DIR)
	$(BIN2S) $< $@ conf_theme_OPL_cfg

$(EE_ASM_DIR)boot.s: misc/boot.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ boot_adp

$(EE_ASM_DIR)cancel.s: misc/cancel.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cancel_adp

$(EE_ASM_DIR)confirm.s: misc/confirm.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ confirm_adp

$(EE_ASM_DIR)cursor.s: misc/cursor.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cursor_adp

$(EE_ASM_DIR)message.s: misc/message.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ message_adp

$(EE_ASM_DIR)transition.s: misc/transition.adp | $(EE_ASM_DIR)
	$(BIN2S) $< $@ transition_adp

$(EE_ASM_DIR)IOPRP_img.s: modules/iopcore/IOPRP.img | $(EE_ASM_DIR)
	$(BIN2S) $< $@ IOPRP_img

$(EE_ASM_DIR)drvtif_ingame_irx.s: modules/debug/drvtif-ingame.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ drvtif_ingame_irx

$(EE_ASM_DIR)tifinet_ingame_irx.s: modules/debug/tifinet-ingame.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ tifinet_ingame_irx

$(EE_ASM_DIR)drvtif_irx.s: modules/debug/drvtif.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ drvtif_irx

$(EE_ASM_DIR)tifinet_irx.s: modules/debug/tifinet.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ tifinet_irx

$(EE_ASM_DIR)deci2_img.s: modules/debug/deci2.img | $(EE_ASM_DIR)
	$(BIN2S) $< $@ deci2_img

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o: $(EE_ASM_DIR)%.s | $(EE_OBJS_DIR)
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

oplversion:
	@echo $(OPL_VERSION)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
