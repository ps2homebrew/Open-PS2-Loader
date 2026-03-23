/*********************************************************************
 * Copyright (C) 2003 Tord Lindstrom (pukko@home.se)
 * This file is subject to the terms and conditions of the PS2Link License.
 * See the file LICENSE in the main directory of this distribution for more
 * details.
 */

#include <stdio.h>
#include <sysclib.h>
#include <loadcore.h>
#include <intrman.h>
#include <types.h>
#include <sifrpc.h>
#include <cdvdman.h>
#include "excepHandler.h"

#define MODNAME "ps2link"
IRX_ID(MODNAME, 1, 8);

// Entry points
extern int fsysMount(void);
extern int cmdHandlerInit(void);
extern int ttyMount(void);
extern int naplinkRpcInit(void);
////////////////////////////////////////////////////////////////////////
// main
//   start threads & init rpc & filesys
int _start(int argc, char **argv)
{
    FlushDcache();
    CpuEnableIntr();

    sceCdInit(1);
    sceCdStop();

    SifInitRpc(0);

    fsysMount();
    printf("host: mounted\n");
    cmdHandlerInit();
    printf("IOP cmd thread started\n");
    naplinkRpcInit();
    printf("Naplink thread started\n");

    installExceptionHandlers();

    return 0;
}
