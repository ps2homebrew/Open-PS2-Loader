#ifndef __TEXTURES_H
#define __TEXTURES_H

#define LOAD0_ICON			 0
#define LOAD1_ICON			 1
#define LOAD2_ICON			 2
#define LOAD3_ICON			 3
#define LOAD4_ICON			 4
#define LOAD5_ICON			 5
#define LOAD6_ICON			 6
#define LOAD7_ICON			 7
#define USB_ICON			 8
#define HDD_ICON			 9
#define ETH_ICON			10
#define APP_ICON			11
#define LEFT_ICON			12
#define RIGHT_ICON			13
#define UP_ICON				14
#define DOWN_ICON			15
#define CROSS_ICON			16
#define TRIANGLE_ICON		17
#define CIRCLE_ICON			18
#define SQUARE_ICON			19
#define L1_ICON				20
#define L2_ICON				21
#define R1_ICON				22
#define R2_ICON				23
#define SELECT_ICON			24
#define START_ICON			25
#define LOGO_PICTURE		26

#define TEXTURES_COUNT		27

#define ERR_BAD_FILE      -1
#define ERR_READ_STRUCT   -2
#define ERR_INFO_STRUCT   -3
#define ERR_SET_JMP       -4
#define ERR_BAD_DIMENSION -5
#define ERR_MISSING_ALPHA -6
#define ERR_BAD_DEPTH     -7

int texPngLoad(GSTEXTURE *texture, char *path, int texId, short psm);
int texJpgLoad(GSTEXTURE* texture, char* path, int texId, short psm);
void texPrepare(GSTEXTURE* texture, short psm);
int texDiscoverLoad(GSTEXTURE* texture, char* path, int texId, short psm);

#endif
