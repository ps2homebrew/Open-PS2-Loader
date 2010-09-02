/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>

#include "smb_fio.h"
#include "smb.h"
#include "debug.h"

#define MODNAME 	"smbman"
#define VER_MAJOR	2
#define VER_MINOR	1

IRX_ID(MODNAME, VER_MAJOR, VER_MINOR);

struct irx_export_table _exp_smbman;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	DPRINTF("%s version 0x%01x%02x start!\n", MODNAME, VER_MAJOR, VER_MINOR);

	RegisterLibraryEntries(&_exp_smbman);

	smb_initdev();

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
	smb_Disconnect();

	return 0;
}

