
EE_BIN = genvmclab.elf
EE_OBJS = genvmclab.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o udptty.o ioptrap.o ps2link.o iomanx.o filexio.o usbd.o usbhdfsd.o mcman.o genvmc.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

BIN2C = $(PS2SDK)/bin/bin2c

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/network/SMSUTILS clean
	$(MAKE) -C ../../modules/network/SMSTCPIP clean
	$(MAKE) -C ../../modules/network/SMSMAP clean
	$(MAKE) -C ../../modules/debug/udptty clean
	$(MAKE) -C ../../modules/debug/ioptrap clean
	$(MAKE) -C ../../modules/debug/ps2link clean
	$(MAKE) -C ../../modules/usb/usbhdfsd clean
	$(MAKE) -C ../../modules/vmc/genvmc clean
	$(MAKE) -C ../../modules/vmc/mcman clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.c: $(PS2SDK)/iop/irx/poweroff.irx
	$(BIN2C) $< $@ $(*F)_irx

ps2dev9.c: ../../modules/dev9/ps2dev9.irx
	$(MAKE) -C ../../modules/dev9
	$(BIN2C) $< $@ $(*F)_irx

smsutils.c: ../../modules/network/SMSUTILS/SMSUTILS.irx
	$(MAKE) -C ../../modules/network/SMSUTILS
	$(BIN2C) $< $@ $(*F)_irx

smstcpip.c: ../../modules/network/SMSTCPIP/SMSTCPIP.irx
	$(MAKE) -C ../../modules/network/SMSTCPIP
	$(BIN2C) $< $@ $(*F)_irx

smsmap.c: ../../modules/network/SMSMAP/SMSMAP.irx
	$(MAKE) -C ../../modules/network/SMSMAP
	$(BIN2C) $< $@ $(*F)_irx

iomanx.c: $(PS2SDK)/iop/irx/iomanX.irx
	$(BIN2C) $< $@ $(*F)_irx

filexio.c: $(PS2SDK)/iop/irx/fileXio.irx
	$(BIN2C) $< $@ $(*F)_irx

usbd.c: $(PS2SDK)/iop/irx/usbd.irx
	$(BIN2C) $< $@ $(*F)_irx

usbhdfsd.c: ../../modules/usb/usbhdfsd/usbhdfsd.irx
	$(MAKE) -C ../../modules/usb/usbhdfsd
	$(BIN2C) $< $@ $(*F)_irx

udptty.c: ../../modules/debug/udptty/udptty.irx
	$(MAKE) -C ../../modules/debug/udptty
	$(BIN2C) $< $@ $(*F)_irx

ioptrap.c: ../../modules/debug/ioptrap/ioptrap.irx
	$(MAKE) -C ../../modules/debug/ioptrap
	$(BIN2C) $< $@ $(*F)_irx

ps2link.c: ../../modules/debug/ps2link/ps2link.irx
	$(MAKE) -C ../../modules/debug/ps2link
	$(BIN2C) $< $@ $(*F)_irx

mcman.c: ../../modules/vmc/mcman/mcman.irx
	$(MAKE) -C ../../modules/vmc/mcman
	$(BIN2C) $< $@ $(*F)_irx

genvmc.c: ../../modules/vmc/genvmc/genvmc.irx
	$(MAKE) -C ../../modules/vmc/genvmc
	$(BIN2C) $< $@ $(*F)_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
