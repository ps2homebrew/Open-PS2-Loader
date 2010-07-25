/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef __GENVMC_H__
#define __GENVMC_H__

// DEVCTL commands
#define GENVMC_DEVCTL_CREATE_VMC	0xC0DE0001

// helpers for DEVCTL commands
typedef struct {		// size = 32
	char VMC_filename[1024];
	int  VMC_size_mb;
	int  VMC_blocksize;
} createVMCparam_t;

#endif
