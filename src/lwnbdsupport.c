// this need to be on top since some include miss including some headers
#include "include/opl.h"

#include "include/config.h"
#include "include/ethsupport.h"
#include "include/extern_irx.h"
#include "include/gui.h" // guiExecDeferredOps
#include "include/hddsupport.h"
#include "include/ioman.h" // ioBlockOps
#include "include/lang.h"
#include "include/lwnbdsupport.h"
#include "include/sound.h"
#include "include/system.h"

static SifRpcClientData_t SifRpcClient;

int NBDInit(void)
{
    int sid = LWNBD_SID;

    while (SifBindRpc(&SifRpcClient, sid, 0) < 0 || SifRpcClient.server == NULL) {
        nopdelay();
    }

    return 0;
}

void NBDDeinit(void)
{
    memset(&SifRpcClient, 0, sizeof(SifRpcClientData_t));
}

static int loadLwnbdSvr(void)
{
    int ret;
    struct lwnbd_config
    {
        char defaultexport[32];
        uint8_t readonly;
    };
    struct lwnbd_config config;
    char gExportName[32];
    config_set_t *configNet = configGetByType(CONFIG_NETWORK);

    configGetStrCopy(configNet, CONFIG_NET_NBD_DEFAULT_EXPORT, gExportName, sizeof(gExportName));

    // deint audio lib while nbd server is running
    //    audioEnd();

    // block all io ops, wait for the ones still running to finish
    //    ioBlockOps(1);
    //    guiExecDeferredOps();

    /* compat stuff for user not providing name export (useless when there was only one export) */
    ret = strlen(gExportName);
    if (ret == 0)
        strcpy(config.defaultexport, "hdd0");
    else
        strcpy(config.defaultexport, gExportName);

    config.readonly = !gEnableWrite;

    if (gNetworkStartup != 0)
        ret = ethLoadInitModules();
    if (ret != 0)
        return -1;

    if (gHDDStartMode != 0)
        hddLoadModules();

    ret = SifCallRpc(&SifRpcClient, LWNBD_SERVER_CMD_START, 0, &config, sizeof(struct lwnbd_config), NULL, 0, NULL, NULL);

    return ret;
}

static void unloadLwnbdSvr(void)
{
    SifCallRpc(&SifRpcClient, LWNBD_SERVER_CMD_STOP, 0, NULL, 0, NULL, 0, NULL, NULL);

    // now start io again
    //    ioBlockOps(0);
    //
    //    audioInit();
    //    sfxInit(0);
    //    if (gEnableBGM)
    //        bgmStart();
}

/* call in src/menusys.c */
void handleLwnbdSrv()
{
    char temp[256];
    // prepare for lwnbd, display screen with info
    guiRenderTextScreen(_l(_STR_STARTINGNBD));
    if (loadLwnbdSvr() == 0) {
        snprintf(temp, sizeof(temp), "%s", _l(_STR_RUNNINGNBD));
        guiMsgBox(temp, 0, NULL);
    } else
        guiMsgBox(_l(_STR_STARTFAILNBD), 0, NULL);

    // restore normal functionality again
    guiRenderTextScreen(_l(_STR_UNLOADNBD));
    unloadLwnbdSvr();
}
