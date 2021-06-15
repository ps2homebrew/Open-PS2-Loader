/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef __GENVMC_H__
#define __GENVMC_H__

// DEVCTL commands
#define GENVMC_DEVCTL_CREATE_VMC 0xC0DE0001
#define GENVMC_DEVCTL_ABORT      0xC0DE0002
#define GENVMC_DEVCTL_STATUS     0xC0DE0003

#define GENVMC_STAT_AVAIL 0x00
#define GENVMC_STAT_BUSY  0x01

// helpers for DEVCTL commands
typedef struct
{ // size = 1036
    char VMC_filename[1024];
    int VMC_size_mb;
    int VMC_blocksize;
    int VMC_thread_priority;
    int VMC_card_slot; // 0=slot 1, 1=slot 2, anything else=blank
} createVMCparam_t;

typedef struct
{                   // size = 76
    int VMC_status; // 0=available, 1=busy
    int VMC_error;
    int VMC_progress;
    char VMC_msg[64];
} statusVMCparam_t;

#endif
