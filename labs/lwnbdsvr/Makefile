
EE_BIN = lwnbdsvrlab.elf
EE_OBJS = lwnbdsvrlab.o discid.o poweroff.o ps2dev9.o smsutils.o smstcpip.o smsmap.o ps2atad.o ps2hdd.o lwnbdsvr.o iomanx.o filexio.o

EE_LIBS = -lfileXio -lpatches -ldebug -lc -lkernel
EE_CFLAGS = -g
EE_LDFLAGS = -s

BIN2C = $(PS2SDK)/bin/bin2c

all:
	$(MAKE) $(EE_BIN)

clean:
	$(MAKE) -C ../../modules/cdvd/discID clean
	$(MAKE) -C ../../modules/dev9 clean
	$(MAKE) -C ../../modules/network/SMSUTILS clean
	$(MAKE) -C ../../modules/network/SMSTCPIP -f Makefile clean
	$(MAKE) -C ../../modules/network/SMSMAP clean
	$(MAKE) -C ../../modules/hdd/atad clean
	$(MAKE) -C ../../modules/hdd/ps2hdd clean
	$(MAKE) -C ../../modules/network/lwNBD clean
	rm -f *.elf *.o *.a *.s

rebuild: clean all

discid.c: ../../modules/cdvd/discID/discID.irx
	$(MAKE) -C ../../modules/cdvd/discID
	$(BIN2C) $< $@ $(*F)

poweroff.c: $(PS2SDK)/iop/irx/poweroff.irx
	$(BIN2C) $< $@ $(*F)

ps2dev9.c: ../../modules/dev9/ps2dev9.irx
	$(MAKE) -C ../../modules/dev9
	$(BIN2C) $< $@ $(*F)

smsutils.c: ../../modules/network/SMSUTILS/SMSUTILS.irx
	$(MAKE) -C ../../modules/network/SMSUTILS
	$(BIN2C) $< $@ $(*F)

smstcpip.c: ../../modules/network/SMSTCPIP/SMSTCPIP.irx
	$(MAKE) -C ../../modules/network/SMSTCPIP -f Makefile rebuild
	$(BIN2C) $< $@ $(*F)

smsmap.c: ../../modules/network/SMSMAP/SMSMAP.irx
	$(MAKE) -C ../../modules/network/SMSMAP
	$(BIN2C) $< $@ $(*F)

ps2atad.c: ../../modules/hdd/atad/ps2atad.irx
	$(MAKE) -C ../../modules/hdd/atad
	$(BIN2C) $< $@ $(*F)

ps2hdd.c: ../../modules/hdd/ps2hdd/ps2hdd.irx
	$(MAKE) -C ../../modules/hdd/ps2hdd
	$(BIN2C) $< $@ $(*F)

lwnbdsvr.c: ../../modules/network/lwNBD/lwnbdsvr.irx
	$(MAKE) -C ../../modules/network/lwNBD
	$(BIN2C) $< $@ $(*F)

iomanx.c: $(PS2SDK)/iop/irx/iomanX.irx
	$(BIN2C) $< $@ $(*F)

filexio.c: $(PS2SDK)/iop/irx/fileXio.irx
	$(BIN2C) $< $@ $(*F)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
