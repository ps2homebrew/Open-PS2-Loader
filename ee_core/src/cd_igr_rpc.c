#include <tamtypes.h>
#include <ps2lib_err.h>
#include <kernel.h>
#include <sifrpc.h>

#include "cd_igr_rpc.h"

//This uses the libcdvd power-off RPC, which only has 1 official function implemented.
int oplIGRShutdown(void)
{
    SifRpcClientData_t _igr_cd;
    int r;

    while ((r = SifBindRpc(&_igr_cd, 0x80000596, 0)) >= 0 && (!_igr_cd.server))
        nopdelay();

    if (r < 0)
        return -E_SIF_RPC_BIND;

    if (SifCallRpc(&_igr_cd, 0x0596, 0, NULL, 0, NULL, 0, NULL, NULL) < 0)
        return -E_SIF_RPC_CALL;

    return 0;
}

