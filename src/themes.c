#include "include/usbld.h"
#include "include/themes.h"
#include "include/util.h"
#include "include/gui.h"
#include "include/renderman.h"
#include "include/textures.h"
#include "include/ioman.h"

#define MENU_POS_H 160
#define MENU_POS_V 50
#define HINT_HEIGHT 32

theme_t* gTheme;

static int screenWidth;
static int screenHeight;
static int guiThemeID = 0;

static int nThemes = 0;
static theme_file_t themes[MAX_THEMES_FILES];
static char **guiThemesNames = NULL;


GSTEXTURE* thmGetTexture(unsigned int id) {
	if (id >= TEXTURES_COUNT)
		return NULL;
	else {
		// see if the texture is valid
		GSTEXTURE* txt = &gTheme->textures[id];
		
		if (txt->Mem)
			return txt;
		else
			return NULL;
	}
}

static void thmFree(theme_t* theme) {
	if (theme) {
		GSTEXTURE* texture;
		int texId = 0;
		for(; texId < TEXTURES_COUNT; texId++) {
			texture = &theme->textures[texId];
			if (texture->Mem != NULL) {
				free(texture->Mem);
				texture->Mem = NULL;
			}
		}

		free(theme);
		theme = NULL;
	}
}

static int thmReadEntry(int index, char* path, char* separator, char* name, unsigned int mode) {
	LOG("thmReadEntry() path=%s sep=%s name=%s\n", path, separator, name);
	if (FIO_SO_ISDIR(mode) && strstr(name, "thm_")) {
		theme_file_t* currTheme = &themes[nThemes + index];

		int length = strlen(name) - 4 + 1;
		currTheme->name = (char*) malloc(length * sizeof(char));
		memcpy(currTheme->name, name + 4, length);
		currTheme->name[length - 1] = '\0';

		length = strlen(path) + 1 + strlen(name) + 1;
		currTheme->filePath = (char*) malloc(length * sizeof(char));
		sprintf(currTheme->filePath, "%s%s%s", path, name, separator);

		LOG("Theme found: %s\n", currTheme->filePath);

		index++;
	}
	return index;
}

/* themePath must contains the leading separator (as it is dependent of the device, we can't know here) */
static int thmLoadResource(int texId, char* themePath, short psm) {
	int success = -1;
	GSTEXTURE* texture = &gTheme->textures[texId];

	if (themePath != NULL)
		success = texDiscoverLoad(texture, themePath, texId, psm); // only set success here

	if ((success < 0) && gTheme->useDefault)
		texPngLoad(texture, NULL, texId, psm);  // we don't mind the result of "default"

	return success;
}

static void getElem(struct TConfigSet* themeConfig, theme_t* theme, theme_element_t* elem, char* name,
		short enabled, int x, int y, short aligned, int w, int h, int defColor) {

	int intValue;
	unsigned char charColor[3];
	char elemProp[64];

	elem->name = name;

	elem->enabled = enabled;
	snprintf(elemProp, 64, "%s_enabled", elem->name);
	if (getConfigInt(themeConfig, elemProp, &intValue))
		elem->enabled = intValue;

	snprintf(elemProp, 64, "%s_x", elem->name);
	if (!getConfigInt(themeConfig, elemProp, &intValue))
		intValue = x;
	if (intValue < 0)
		elem->posX = screenWidth + intValue;
	else
		elem->posX = intValue;

	snprintf(elemProp, 64, "%s_y", elem->name);
	if (!getConfigInt(themeConfig, elemProp, &intValue))
		intValue = y;
	if (intValue < 0)
		elem->posY = ceil((screenHeight + intValue) * theme->usedHeight / screenHeight);
	else
		elem->posY = intValue;

	elem->aligned = aligned;
	snprintf(elemProp, 64, "%s_aligned", elem->name);
	if (getConfigInt(themeConfig, elemProp, &intValue))
		elem->aligned = intValue;

	snprintf(elemProp, 64, "%s_width", elem->name);
	if (!getConfigInt(themeConfig, elemProp, &intValue))
		intValue = w;
	elem->width = intValue;
	if (intValue < 0) {
		if (intValue == DIM_INF) {
			if (elem->aligned) // incompatible, so set to unaligned
				elem->aligned = ALIGN_NONE;
		}
	}

	snprintf(elemProp, 64, "%s_height", elem->name);
	if (!getConfigInt(themeConfig, elemProp, &intValue))
		intValue = h;
	elem->height = intValue;
	if (intValue < 0) {
		if (intValue == DIM_INF) {
			if (elem->aligned) // incompatible, so reset to unaligned
				elem->aligned = ALIGN_NONE;
		}
	}

	if (defColor)
		elem->color = gDefaultCol;
	else
		elem->color = theme->textColor;
	snprintf(elemProp, 64, "%s_color", elem->name);
	if (getConfigColor(themeConfig, elemProp, charColor))
		elem->color = GS_SETREG_RGBA(charColor[0], charColor[1], charColor[2], 0xff);
}

static void setColors(theme_t* theme) {
	memcpy(theme->bgColor, gDefaultBgColor, 3);
	theme->textColor = GS_SETREG_RGBA(gDefaultTextColor[0], gDefaultTextColor[1], gDefaultTextColor[2], 0xff);
	theme->uiTextColor = GS_SETREG_RGBA(gDefaultUITextColor[0], gDefaultUITextColor[1], gDefaultUITextColor[2], 0xff);
	theme->selTextColor = GS_SETREG_RGBA(gDefaultSelTextColor[0], gDefaultSelTextColor[1], gDefaultSelTextColor[2], 0xff);
}

static void thmLoad(char* themePath) {
	LOG("thmLoad() path=%s\n", themePath);
	theme_t* curT = gTheme;
	theme_t* newT = (theme_t*) malloc(sizeof(theme_t));
	memset(newT, 0, sizeof(theme_t));

	struct TConfigSet themeConfig;
	themeConfig.head = NULL;
	themeConfig.tail = NULL;

	if (themePath) {
		char path[255];
		snprintf(path, 255, "%sconf_theme.cfg", themePath);
		readConfig(&themeConfig, path); // try to load the theme config file
	}

	int intValue;
	newT->useDefault = 1;
	if (getConfigInt(&themeConfig, "use_default", &intValue))
		newT->useDefault = intValue;

	newT->itemsListIcons = 0;
	if (getConfigInt(&themeConfig, "items_list_icons", &intValue))
		newT->itemsListIcons = intValue;

	newT->usedHeight = 480;
	if (getConfigInt(&themeConfig, "use_real_height", &intValue))
		if (intValue)
			newT->usedHeight = screenHeight;

	newT->displayedItems = (newT->usedHeight - (MENU_POS_V + HINT_HEIGHT)) / MENU_ITEM_HEIGHT;
	if (getConfigInt(&themeConfig, "displayed_items", &intValue)) {
		if (intValue < newT->displayedItems)
			newT->displayedItems = intValue;
	}

	newT->drawBackground = &guiDrawBGPicture;
	newT->drawAltBackground = &guiDrawBGPlasma;
	if (getConfigInt(&themeConfig, "background_mode", &intValue)) {
		if (intValue == BG_MODE_COLOR)
			newT->drawBackground = &guiDrawBGColor;
		else if (intValue == BG_MODE_PLASMA)
			newT->drawBackground = &guiDrawBGPlasma;
		else if (intValue == BG_MODE_ART)
			newT->drawBackground = &guiDrawBGArt;
	}

	setColors(newT);
	getConfigColor(&themeConfig, "bg_color", newT->bgColor);

	unsigned char color[3];
	if (getConfigColor(&themeConfig, "text_color", color))
		newT->textColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

	if (getConfigColor(&themeConfig, "ui_text_color", color))
		newT->uiTextColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

	if (getConfigColor(&themeConfig, "sel_text_color", color))
		newT->selTextColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

	getElem(&themeConfig, newT, &newT->menuIcon, "menu_icon", 1, 40, 40, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, 1);
	getElem(&themeConfig, newT, &newT->menuText, "menu_text", 1, screenWidth >> 1, 20, ALIGN_CENTER, 200, 20, 0);
	getElem(&themeConfig, newT, &newT->itemsList, "items_list", 1, 150, MENU_POS_V, ALIGN_NONE, DIM_INF, newT->displayedItems * MENU_ITEM_HEIGHT, 0);
	getElem(&themeConfig, newT, &newT->itemIcon, "item_icon", 1, 80, newT->usedHeight >> 1, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, 1);
	getElem(&themeConfig, newT, &newT->itemCover, "item_cover", 1, 520, newT->usedHeight >> 1, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, 1);
	getElem(&themeConfig, newT, &newT->itemText, "item_text", 1, 520, 370, ALIGN_CENTER, DIM_UNDEF, 20, 0);
	getElem(&themeConfig, newT, &newT->hintText, "hint_text", 1, 16, -HINT_HEIGHT, ALIGN_NONE, DIM_INF, HINT_HEIGHT, 0);
	getElem(&themeConfig, newT, &newT->loadingIcon, "loading_icon", 1, -50, -50, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, 1);

	if (getConfigInt(&themeConfig, "cover_blend_ulx", &intValue))
		newT->coverBlend_ulx = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_uly", &intValue))
		newT->coverBlend_uly = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_urx", &intValue))
		newT->coverBlend_urx = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_ury", &intValue))
		newT->coverBlend_ury = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_blx", &intValue))
		newT->coverBlend_blx = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_bly", &intValue))
		newT->coverBlend_bly = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_brx", &intValue))
		newT->coverBlend_brx = intValue;
	if (getConfigInt(&themeConfig, "cover_blend_bry", &intValue))
		newT->coverBlend_bry = intValue;

	newT->busyIconsCount = LOAD7_ICON - LOAD0_ICON + 1;

	/// Now swap theme and start loading textures
	if (newT->usedHeight == screenHeight)
		rmResetShiftRatio();
	else
		rmSetShiftRatio((float) screenHeight / newT->usedHeight);
	
	gTheme = newT;
	thmFree(curT);
	
	int i;
	
	// default all to not loaded...
	for (i = 0; i < TEXTURES_COUNT; i++) {
		gTheme->textures[i].Mem = NULL;
	}

	// First start with busy icon
	char* path = themePath;
	int customBusy = 0;
	for (i = LOAD0_ICON; i <= LOAD7_ICON; i++) {
		if (thmLoadResource(i, path, GS_PSM_CT32) >= 0)
			customBusy = 1;
		else {
			if (customBusy)
				break;
			else
				path = NULL;
		}
	}
	gTheme->busyIconsCount = i;

	// Customizable icons
	for (i = EXIT_ICON; i <= DOWN_ICON; i++)
		thmLoadResource(i, themePath, GS_PSM_CT32);

	// Not  customizable icons
	for (i = L1_ICON; i <= START_ICON; i++)
		thmLoadResource(i, NULL, GS_PSM_CT32);

	// Finish with background
	thmLoadResource(COVER_OVERLAY, themePath, GS_PSM_CT32);
	thmLoadResource(BACKGROUND_PICTURE, themePath, GS_PSM_CT24);

	// LOGO is hardcoded
	thmLoadResource(LOGO_PICTURE, NULL, GS_PSM_CT24);
	
	// TODO: loadFont();
}

static void thmRebuildGuiNames() {
	if (guiThemesNames)
		free(guiThemesNames);

	// build the languages name list
	guiThemesNames = (char**) malloc((nThemes + 2) * sizeof(char**));

	// add default internal
	guiThemesNames[0] = "<OPL>";

	int i = 0;
	for (; i < nThemes; i++) {
		guiThemesNames[i + 1] =  themes[i].name;
	}

	guiThemesNames[nThemes + 1] = NULL;
}

void thmAddElements(char* path, char* separator) {
	LOG("thmAddElements() path=%s sep=%s\n", path, separator);
	nThemes += listDir(path, separator, MAX_THEMES_FILES - nThemes, &thmReadEntry);
	LOG("thmAddElements() nThemes=%d\n", nThemes);
	thmRebuildGuiNames();

	char* temp;
	if (getConfigStr(&gConfig, "theme", &temp)) {
		LOG("Trying to set again theme: %s\n", temp);
		thmSetGuiValue(thmFindGuiID(temp));
	}
}

void thmInit() {
	LOG("thmInit()\n");
	gTheme = NULL;

	rmGetScreenExtents(&screenWidth, &screenHeight);

	// initialize default internal
	thmLoad(NULL);

	char path[32];
	sprintf(path, "%s/", gBaseMCDir);
	thmAddElements(path, "/");
}

char* thmGetValue() {
	LOG("thmGetValue() id=%d name=%s\n", guiThemeID, guiThemesNames[guiThemeID]);
	return guiThemesNames[guiThemeID];
}

void thmSetGuiValue(int themeID) {
	LOG("thmSetGuiValue() id=%d\n", themeID);
	if (guiThemeID != themeID) {
		if (themeID != 0)
			thmLoad(themes[themeID - 1].filePath);
		else
			thmLoad(NULL);
		guiThemeID = themeID;
	}
	else if (guiThemeID == 0)
		setColors(gTheme);
}

int thmGetGuiValue() {
	LOG("thmGetGuiValue() id=%d\n", guiThemeID);
	return guiThemeID;
}

int thmFindGuiID(char* theme) {
	LOG("thmFindGuiID() theme=%s\n", theme);
	if (theme) {
		int i = 0;
		for (; i < nThemes; i++) {
			if (stricmp(themes[i].name, theme) == 0)
				return i + 1;
		}
	}
	return 0;
}

char **thmGetGuiList() {
	return guiThemesNames;
}

void thmEnd() {
	thmFree(gTheme);

	int i = 0;
	for (; i < nThemes; i++) {
		free(themes[i].name);
		free(themes[i].filePath);
	}

	free(guiThemesNames);
}
