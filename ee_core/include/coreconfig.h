#ifndef EE_CORE_CONFIG_H_
#define EE_CORE_CONFIG_H_

#include <osd_config.h>
#define CORE_EXIT_PATH_MAX_LEN      (256)
#define CORE_GAME_MODE_DESC_MAX_LEN (16)
#define CORE_GAME_ID_MAX_LEN        (16)

// Do not use "EE" as it might catch a string.
#define EE_CORE_MAGIC_0 (0x4D614730)
#define EE_CORE_MAGIC_1 (0x4D616731)
// #include "ee_core.h"

struct GsmConfig_t
{
    s16 interlace;
    s16 mode;
    s16 ffmd;
    u32 dx_offset;
    u32 dy_offset;
    u64 display;
    u64 syncv;
    u64 smode2;
    int k576P_fix;
    int kGsDxDyOffsetSupported;
    int FIELD_fix;
};


struct EECoreConfig_t
{
    u32 magic[2];

    char GameMode;
    char GameModeDesc[CORE_GAME_MODE_DESC_MAX_LEN];

    int EnableDebug;

    char ExitPath[CORE_EXIT_PATH_MAX_LEN];

    int HDDSpindown;

    char g_ps2_ip[16];
    char g_ps2_netmask[16];
    char g_ps2_gateway[16];
    unsigned char g_ps2_ETHOpMode;

    unsigned int *gCheatList; // Store hooks/codes addr+val pairs

    void *eeloadCopy;
    void *initUserMemory;

    void *ModStorageStart;
    void *ModStorageEnd;

    char GameID[CORE_GAME_ID_MAX_LEN];

    u32 _CompatMask;

    ConfigParam CustomOSDConfigParam;
    int enforceLanguage;

    // Keep them eitherway.
    int EnablePadEmuOp;
    int PadEmuSettings;
    int PadMacroSettings;

    int EnableGSMOp;
    struct GsmConfig_t GsmConfig;
};

#define USE_LOCAL_EECORE_CONFIG struct EECoreConfig_t *config = &g_ee_core_config;
extern struct EECoreConfig_t g_ee_core_config;
#endif
