#include <loadcore.h>
#include <bdm.h>
#include <sifcmd.h>
#include <irx.h>

IRX_ID("bdmevent", 1, 1);

static void bdm_callback(int cause)
{
    static SifCmdHeader_t EventCmdData;

    EventCmdData.opt = cause;
    sceSifSendCmd(0, &EventCmdData, sizeof(EventCmdData), NULL, NULL, 0);
}

int _start(int argc, char *argv[])
{
    bdm_RegisterCallback(&bdm_callback);
    return MODULE_RESIDENT_END;
}
