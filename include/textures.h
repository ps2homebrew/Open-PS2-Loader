#ifndef __TEXTURES_H
#define __TEXTURES_H

enum INTERNAL_TEXTURE {
    LOAD0_ICON = 0,
    LOAD1_ICON,
    LOAD2_ICON,
    LOAD3_ICON,
    LOAD4_ICON,
    LOAD5_ICON,
    LOAD6_ICON,
    LOAD7_ICON,
    BDM_ICON,
    USB_ICON,
    ILINK_ICON,
    MX4SIO_ICON,
    HDD_BD_ICON,
    HDD_ICON,
    ETH_ICON,
    APP_ICON,
    INDEX_0,
    INDEX_1,
    INDEX_2,
    INDEX_3,
    INDEX_4,
    LEFT_ICON,
    RIGHT_ICON,
    CROSS_ICON,
    TRIANGLE_ICON,
    CIRCLE_ICON,
    SQUARE_ICON,
    SELECT_ICON,
    START_ICON,
    /* currently unused.
    UP_ICON,
    DOWN_ICON,
    L1_ICON,
    L2_ICON,
    L3_ICON,
    R1_ICON,
    R2_ICON,
    R3_ICON, */
    MAIN_BG,
    INFO_BG,
    COVER_DEFAULT,
    DISC_DEFAULT,
    SCREEN_DEFAULT,
    ELF_FORMAT,
    HDL_FORMAT,
    ISO_FORMAT,
    ZSO_FORMAT,
    UL_FORMAT,
    APP_MEDIA,
    CD_MEDIA,
    DVD_MEDIA,
    ASPECT_STD,
    ASPECT_WIDE,
    ASPECT_WIDE1,
    ASPECT_WIDE2,
    DEVICE_1,
    DEVICE_2,
    DEVICE_3,
    DEVICE_4,
    DEVICE_5,
    DEVICE_6,
    DEVICE_ALL,
    RATING_0,
    RATING_1,
    RATING_2,
    RATING_3,
    RATING_4,
    RATING_5,
    SCAN_240P,
    SCAN_240P1,
    SCAN_480I,
    SCAN_480P,
    SCAN_480P1,
    SCAN_480P2,
    SCAN_480P3,
    SCAN_480P4,
    SCAN_480P5,
    SCAN_576I,
    SCAN_576P,
    SCAN_720P,
    SCAN_1080I,
    SCAN_1080I2,
    SCAN_1080P,
    VMODE_MULTI,
    VMODE_NTSC,
    VMODE_PAL,
    LOGO_PICTURE,
    CASE_OVERLAY,
    APPS_CASE_OVERLAY,

    TEXTURES_COUNT
};

#define ERR_BAD_FILE      -1
#define ERR_READ_STRUCT   -2
#define ERR_INFO_STRUCT   -3
#define ERR_SET_JMP       -4
#define ERR_BAD_DIMENSION -5
#define ERR_MISSING_ALPHA -6
#define ERR_BAD_DEPTH     -7

int texLookupInternalTexId(const char *name);
int texLoadInternal(GSTEXTURE *texture, int texId);
int texDiscoverLoad(GSTEXTURE *texture, const char *path, int texId);
void texFree(GSTEXTURE *texture);

#endif
