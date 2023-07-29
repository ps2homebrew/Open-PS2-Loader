#include <tamtypes.h>
#include <ps2lib_err.h>
#include <kernel.h>
#include <sifrpc.h>

#include "cd_igr_rpc.h"

int oplIGRShutdown(int poff)
{
    SifRpcClientData_t _igr_cd __attribute__((aligned(64)));
    int r;
    s32 poffData __attribute__((aligned(64)));

    _igr_cd.server = NULL;
    while ((r = SifBindRpc(&_igr_cd, 0x80000598, 0)) >= 0 && (!_igr_cd.server))
        nopdelay();

    if (r < 0)
        return -E_SIF_RPC_BIND;

    *(s32 *)UNCACHED_SEG(&poffData) = poff;
    if (SifCallRpc(&_igr_cd, 1, SIF_RPC_M_NOWBDC, &poffData, sizeof(poffData), NULL, 0, NULL, NULL) < 0)
        return -E_SIF_RPC_CALL;

    return 0;
}
