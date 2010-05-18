/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>

#include "smb_fio.h"
#include "smb.h"

#define MODNAME "smbman"
IRX_ID(MODNAME, 2, 1);

struct irx_export_table _exp_smbman;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
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

