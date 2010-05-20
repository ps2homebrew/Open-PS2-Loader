/*
  Copyright 2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include <loadcore.h>
#include <stdio.h>
#include <ps2ip.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>
#include <errno.h>

#define MODNAME "hdldsvr"
IRX_ID(MODNAME, 1, 1);

struct irx_export_table _exp_hdldsvr;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	RegisterLibraryEntries(&_exp_hdldsvr);

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
	return 0;
}

