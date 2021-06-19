/*
  Copyright 2009-2010, jimmikaelkael <jimmikaelkael@wanadoo.fr>
  Copyright 2009-2010, misfire <misfire@xploderfreax.de>

  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <tamtypes.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <string.h>
#include <stdio.h>

/*static inline void _strcpy(char *dst, const char *src)
{
    memcpy(dst, src, strlen(src) + 1);
}

static inline void _strcat(char *dst, const char *src)
{
    _strcpy(&dst[strlen(dst)], src);
}

static int _strncmp(const char *s1, const char *s2, int length)
{
    const char *a = s1;
    const char *b = s2;

    while (length > 0) {
        if ((*a == 0) || (*b == 0))
            return -1;
        if (*a++ != *b++)
            return 1;
        length--;
    }

    return 0;
}*/

static inline void BootError(char *filename)
{
    char *argv[2];
    argv[0] = "BootError";
    argv[1] = filename;

    ExecOSD(2, argv);
}

static inline void InitializeUserMemory(unsigned int start, unsigned int end)
{
    unsigned int i;

    for (i = start; i < end; i += 64) {
        asm(
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n" ::"r"(i));
    }
}

int main(int argc, char *argv[])
{
    int result;
    t_ExecData exd;

    SifInitRpc(0);

    exd.epc = 0;

    //clear memory.
    InitializeUserMemory(0x00100000, GetMemorySize());
    FlushCache(0);

    SifLoadFileInit();

    result = SifLoadElf(argv[0], &exd);

    SifLoadFileExit();

    if (result == 0 && exd.epc != 0) {
        //Final IOP reset, to fill the IOP with the default modules.
        while (!SifIopReset("", 0)) {
        };

        FlushCache(0);
        FlushCache(2);

        while (!SifIopSync()) {
        };

        SifInitRpc(0);
        //Load modules.
        SifLoadFileInit();
        SifLoadModule("rom0:SIO2MAN", 0, NULL);
        SifLoadModule("rom0:MCMAN", 0, NULL);
        SifLoadModule("rom0:MCSERV", 0, NULL);
        SifLoadFileExit();
        SifExitRpc();

        ExecPS2((void *)exd.epc, (void *)exd.gp, argc, argv);
    } else {
        SifExitRpc();
    }

    BootError(argv[0]);

    return 0;
}
