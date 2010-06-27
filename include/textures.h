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
#define EXIT_ICON			 8
#define CONFIG_ICON			 9
#define SAVE_ICON			10
#define USB_ICON			11
#define HDD_ICON			12
#define ETH_ICON			13
#define APP_ICON			14
#define DISC_ICON			15
#define CROSS_ICON			16
#define TRIANGLE_ICON		17
#define CIRCLE_ICON			18
#define SQUARE_ICON			19
#define SELECT_ICON			20
#define START_ICON			21
#define LEFT_ICON			22
#define RIGHT_ICON			23
#define UP_ICON				24
#define DOWN_ICON			25
#define L1_ICON				26
#define L2_ICON				27
#define R1_ICON				28
#define R2_ICON				29
#define COVER_OVERLAY		30
#define BACKGROUND_PICTURE 31
#define LOGO_PICTURE 32

#define TEXTURES_COUNT 33

#define ERR_BAD_FILE      -1
#define ERR_READ_STRUCT   -2
#define ERR_INFO_STRUCT   -3
#define ERR_SET_JMP       -4
#define ERR_BAD_DIMENSION -5
#define ERR_MISSING_ALPHA -6
#define ERR_BAD_DEPTH     -7

int texPngLoad(GSTEXTURE *texture, char *path, int texId, short psm);
int texJpgLoad(GSTEXTURE* texture, char* path, int texId, short psm);
int texDiscoverLoad(GSTEXTURE* texture, char* path, int texId, short psm);

#endif
