#include <loadcore.h>
#include <irx.h>

#include "oplsmb.h"
#include "smbauth.h"

#define MODNAME "smb_init"
IRX_ID(MODNAME, 1, 1);

int _start(int argc, char *argv[])
{
    smb_NegotiateProt(&SmbInitHashPassword);

    return MODULE_NO_RESIDENT_END;
}
