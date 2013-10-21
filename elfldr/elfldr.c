/*
  Copyright 2009-2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright 2009-2010, misfire <misfire@xploderfreax.de>

  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <tamtypes.h>
#include <kernel.h>
#include <stdio.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <string.h>

#define GS_BGCOLOUR *((volatile unsigned long int*)0x120000E0)

#define ELF_MAGIC	0x464c457f
#define ELF_PT_LOAD	1

typedef struct {
	u8	ident[16];
	u16	type;
	u16	machine;
	u32	version;
	u32	entry;
	u32	phoff;
	u32	shoff;
	u32	flags;
	u16	ehsize;
	u16	phentsize;
	u16	phnum;
	u16	shentsize;
	u16	shnum;
	u16	shstrndx;
} elf_header_t;

typedef struct {
	u32	type;
	u32	offset;
	void	*vaddr;
	u32	paddr;
	u32	filesz;
	u32	memsz;
	u32	flags;
	u32	align;
} elf_pheader_t;

#define MAX_G_ARGS 16
static int g_argc;
static char *g_argv[MAX_G_ARGS];
static char g_argbuf[1024];

static void t_loadElf(void)
{
	int i, fd;
	elf_header_t elf_header;
	elf_pheader_t elf_pheader;

#ifdef DEBUG
	GS_BGCOLOUR = 0x800080; // mid purple
#endif

	ResetEE(0x7f);

	// wipe user memory
	for (i = 0x00100000; i < 0x02000000; i += 64) {
		__asm__ (
			"\tsq $0, 0(%0) \n"
			"\tsq $0, 16(%0) \n"
			"\tsq $0, 32(%0) \n"
			"\tsq $0, 48(%0) \n"
			:: "r" (i)
		);
	}

	// clear scratchpad memory
	memset((void*)0x70000000, 0, 16 * 1024);

#ifdef DEBUG
	GS_BGCOLOUR = 0x008000; // mid green
#endif

	// load the elf
	fioInit();
 	fd = open(g_argv[0], O_RDONLY);
 	if (fd < 0) {
		goto error; // can't open file, exiting...
 	}

	// read ELF header
	if (read(fd, &elf_header, sizeof(elf_header)) != sizeof(elf_header)) {
		close(fd);
		goto error; // can't read header, exiting...
	}

	// check ELF magic
	if ((*(u32*)elf_header.ident) != ELF_MAGIC) {
		close(fd);
		goto error; // not an ELF file, exiting...
	}

	// copy loadable program segments to RAM
	for (i = 0; i < elf_header.phnum; i++) {
		lseek(fd, elf_header.phoff+(i*sizeof(elf_pheader)), SEEK_SET);
		read(fd, &elf_pheader, sizeof(elf_pheader));

		if (elf_pheader.type != ELF_PT_LOAD)
			continue;

		lseek(fd, elf_pheader.offset, SEEK_SET);
		read(fd, elf_pheader.vaddr, elf_pheader.filesz);

		if (elf_pheader.memsz > elf_pheader.filesz)
			memset(elf_pheader.vaddr + elf_pheader.filesz, 0,
					elf_pheader.memsz - elf_pheader.filesz);
	}

	close(fd);

#ifdef RESET_IOP
	// reset IOP
	SifInitRpc(0);
	SifResetIop();
	SifInitRpc(0);

	FlushCache(0);
	FlushCache(2);

	// reload modules
	SifLoadFileInit();
	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:MCSERV", 0, NULL);
#endif

	// exit services
	fioExit();
	SifLoadFileExit();
	SifExitIopHeap();
	SifExitRpc();

	FlushCache(0);
	FlushCache(2);

#ifdef DEBUG
	GS_BGCOLOUR = 0x000000; // black
#endif

	// finally, run ELF...
	ExecPS2((void*)elf_header.entry, NULL, g_argc, g_argv);
error:
	GS_BGCOLOUR = 0x808080; // gray screen: error
	SleepThread();
}

int main(int argc, char **argv)
{
	char *p = g_argbuf;
	int i, arglen;

#ifdef DEBUG
	GS_BGCOLOUR = 0x000080; // mid blue
#endif

	DI();
	ee_kmode_enter();

	// check args count
	g_argc = argc > MAX_G_ARGS ? MAX_G_ARGS : argc;

	memset(g_argbuf, 0, sizeof(g_argbuf));

	// copy args
	g_argv[0] = p;
	for (i = 0; i < g_argc; i++) {
		arglen = strlen(argv[i]) + 1;
		memcpy(p, argv[i], arglen);
		g_argv[i + 1] = p;
		p += arglen;
	}

	ee_kmode_exit();
	EI();

	if(*(volatile u32 *)(0x0008000C)) {
		/* Starting GSM */
		void (*StartGSM)(void);
		StartGSM = (void *)*(volatile u32 *)(0x00080004);
		StartGSM();
	}

	ExecPS2(t_loadElf, NULL, 0, NULL);

	GS_BGCOLOUR = 0xffffff; // white screen: error
	SleepThread();

	return 0;
}

