/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$
*/

#ifndef _JOURNAL_H
#define _JOURNAL_H

#define APAL_MAGIC			0x4150414C	// 'APAL'
typedef struct
{
	u32		magic;			// APAL_MAGIC
	s32		num;
	u32		sectors[126];
} apa_journal_t;

///////////////////////////////////////////////////////////////////////////////
//   Function declerations

#define journalCheckSum(header) apaCheckSum((apa_header *)header)
int journalReset(u32 device);
int journalFlush(u32 device);
int journalWrite(apa_cache *clink);
int journalResetore(u32 device);

#endif /* _JOURNAL_H */

