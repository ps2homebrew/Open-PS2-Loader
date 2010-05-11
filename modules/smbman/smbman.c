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

#define MAX_SECTORS 	7

static char g_pc_ip[]="xxx.xxx.xxx.xxx";
static int g_pc_port = 445; // &g_pc_ip + 16
static char g_pc_share[33]="PS2SMB";

int smbman_initdev(void);

struct irx_export_table _exp_smbman;

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{
	char tree_str[255];

	RegisterLibraryEntries(&_exp_smbman);

	// Open the Connection with SMB server
	smb_NegociateProtocol(g_pc_ip, g_pc_port, "NT LM 0.12");

	// zero pad the string to be sure it does not overflow
	g_pc_share[32] = '\0';

	// Then open a session and a tree connect on the share resource
	sprintf(tree_str, "\\\\%s\\%s", g_pc_ip, g_pc_share);
	smb_SessionSetupAndX("GUEST", NULL, 0);
	smb_TreeConnectAndX(tree_str, NULL, 0);
	//smb_TreeConnectAndX("\\\\192.168.0.2\\IPC$", NULL, 0);
	//smb_NetShareEnum();

	smb_initdev();

	return MODULE_RESIDENT_END;
}

//-------------------------------------------------------------------------
int _shutdown(void)
{
	smb_Disconnect();

	return 0;
}

