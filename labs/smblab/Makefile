
EE_BIN = smblab.elf
EE_OBJS = smblab.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o smbman.o iomanx.o filexio.o udptty.o ioptrap.o ps2link.o

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
	rm -f *.elf *.o *.a *.s

rebuild: clean all

poweroff.c: $(PS2SDK)/iop/irx/poweroff.irx
	$(BIN2C) $< $@ $(*F)

ps2dev9.c: ../../modules/dev9/ps2dev9.irx
	$(MAKE) -C ../../modules/dev9
	$(BIN2C) $< $@ $(*F)

smsutils.c: ../../modules/network/SMSUTILS/SMSUTILS.irx
	$(MAKE) -C ../../modules/network/SMSUTILS
	$(BIN2C) $< $@ $(*F)

smstcpip.c: ../../modules/network/SMSTCPIP/SMSTCPIP.irx
	$(MAKE) -C ../../modules/network/SMSTCPIP
	$(BIN2C) $< $@ $(*F)

smsmap.c: ../../modules/network/SMSMAP/SMSMAP.irx
	$(MAKE) -C ../../modules/network/SMSMAP
	$(BIN2C) $< $@ $(*F)

smbman.c: $(PS2SDK)/iop/irx/smbman.irx
	$(BIN2C) $< $@ $(*F)

udptty.c: ../../modules/debug/udptty/udptty.irx
	$(MAKE) -C ../../modules/debug/udptty
	$(BIN2C) $< $@ $(*F)

ioptrap.c: ../../modules/debug/ioptrap/ioptrap.irx
	$(MAKE) -C ../../modules/debug/ioptrap
	$(BIN2C) $< $@ $(*F)

ps2link.c: ../../modules/debug/ps2link/ps2link.irx
	$(MAKE) -C ../../modules/debug/ps2link
	$(BIN2C) $< $@ $(*F)

iomanx.c: $(PS2SDK)/iop/irx/iomanX.irx
	$(BIN2C) $< $@ $(*F)

filexio.c: $(PS2SDK)/iop/irx/fileXio.irx
	$(BIN2C) $< $@ $(*F)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
