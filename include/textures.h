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
    USB_ICON,
    HDD_ICON,
    ETH_ICON,
    APP_ICON,
    LEFT_ICON,
    RIGHT_ICON,
    UP_ICON,
    DOWN_ICON,
    CROSS_ICON,
    TRIANGLE_ICON,
    CIRCLE_ICON,
    SQUARE_ICON,
    SELECT_ICON,
    START_ICON,
    L1_ICON,
    L2_ICON,
    R1_ICON,
    R2_ICON,
    LOGO_PICTURE,
    BG_OVERLAY,

    TEXTURES_COUNT
};

#define ERR_BAD_FILE -1
#define ERR_READ_STRUCT -2
#define ERR_INFO_STRUCT -3
#define ERR_SET_JMP -4
#define ERR_BAD_DIMENSION -5
#define ERR_MISSING_ALPHA -6
#define ERR_BAD_DEPTH -7

int texLookupInternalTexId(const char *name);
int texPngLoad(GSTEXTURE *texture, const char *path, int texId, short psm);
int texJpgLoad(GSTEXTURE *texture, const char *path, int texId, short psm);
int texBmpLoad(GSTEXTURE *texture, const char *path, int texId, short psm);
void texPrepare(GSTEXTURE *texture, short psm);
int texDiscoverLoad(GSTEXTURE *texture, const char *path, int texId, short psm);

#endif
