ISO=TEST_OPL.00.OPLtester_v0.5.iso

all:
	$(MAKE) -C iop/freeram all
	$(MAKE) -C ee all

clean:
	$(MAKE) -C iop/freeram clean
	$(MAKE) -C ee clean
	rm -f *.iso
	rm -f iso/*.BIN
	rm -f iso/TEST_OPL.00

run: all
	ps2client -h 192.168.1.10 execee host:ee/opltester.elf

iso: all
	cp ee/opltester.elf iso/TEST_OPL.00
	dd if=/dev/random of=iso/RANDOM.BIN bs=1M count=5
	dd if=/dev/zero   of=iso/ZERO.BIN   bs=1M count=5
	mkisofs -o $(ISO) iso
