/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG
#define DPRINTF(args...)	printf(args)
#else
#define DPRINTF(args...)	do { } while(0)
#endif

#endif
