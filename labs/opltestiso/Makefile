ISO ?= TEST_OPL.00.OPLtester_v0.5.iso

SUPPORT_8MB_IOP_RAM ?= 0

ifeq ($(SUPPORT_8MB_IOP_RAM), 1)
  $(info -- building test iso with support for 8mb IOP RAM machines)
  IOPFLAGS += "IOP_MEM_SIZE=8"
endif

all:
	$(MAKE) -C iop/freeram all $(IOPFLAGS)
	$(MAKE) -C ee all

clean:
	$(MAKE) -C iop/freeram clean $(IOPFLAGS)
	$(MAKE) -C ee clean
	rm -f $(ISO)
	rm -f iso/*.BIN
	rm -f iso/TEST_OPL.00

run: all
	ps2client -h 192.168.1.10 execee host:ee/opltester.elf

iso: all
	cp ee/opltester.elf iso/TEST_OPL.00
	dd if=/dev/random of=iso/RANDOM.BIN bs=1M count=5
	dd if=/dev/zero   of=iso/ZERO.BIN   bs=1M count=5
	mkisofs -o $(ISO) iso
