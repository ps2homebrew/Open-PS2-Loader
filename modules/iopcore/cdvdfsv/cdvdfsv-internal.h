/*
  Copyright 2009, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review open-ps2-loader README & LICENSE files for further details.
*/

#ifndef __CDVDFSV_INTERNAL__
#define __CDVDFSV_INTERNAL__

#include <intrman.h>
#include <loadcore.h>
#include <stdio.h>
#include <sifcmd.h>
#include <sifman.h>
#include <sysclib.h>
#include <thbase.h>
#include <thevent.h>
#include <thsemap.h>
#include <cdvdman.h>

#include "cdvdman_opl.h"

#include "smsutils.h"

#ifdef __IOPCORE_DEBUG
#define DPRINTF(args...)  printf(args)
#define iDPRINTF(args...) Kprintf(args)
#else
#define DPRINTF(args...)
#define iDPRINTF(args...)
#endif

extern void cdvdfsv_register_scmd_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void cdvdfsv_register_ncmd_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void cdvdfsv_register_searchfile_rpc(SifRpcDataQueue_t *rpc_DQ);
extern void sysmemSendEE(void *buf, void *EE_addr, int size);
extern int sceCdChangeThreadPriority(int priority);
extern u8 *cdvdfsv_buf;

#endif
