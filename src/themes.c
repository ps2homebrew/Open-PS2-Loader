#include "include/usbld.h"
#include "include/themes.h"
#include "include/util.h"
#include "include/gui.h"
#include "include/renderman.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/fntsys.h"
#include "include/lang.h"

#define MENU_POS_V 50
#define HINT_HEIGHT 32

theme_t* gTheme;

static int screenWidth;
static int screenHeight;
static int guiThemeID = 0;

static int nThemes = 0;
static theme_file_t themes[THM_MAX_FILES];
static char **guiThemesNames = NULL;

#define TYPE_ATTRIBUTE_TEXT		0
#define TYPE_ATTRIBUTE_IMAGE	1
#define TYPE_GAME_IMAGE			2
#define TYPE_STATIC_IMAGE		3
#define TYPE_BACKGROUND			4
#define TYPE_MENU_ICON			5
#define TYPE_MENU_TEXT			6
#define	 TYPE_ITEMS_LIST		7
#define	 TYPE_ITEM_ICON			8
#define	 TYPE_ITEM_COVER		9
#define TYPE_ITEM_TEXT			10
#define TYPE_HINT_TEXT			11
#define TYPE_LOADING_ICON		12

static char *elementsType[] = {
	"AttributeText",
	"AttributeImage",
	"GameImage",
	"StaticImage",
	"Background",
	"MenuIcon",
	"MenuText",
	"ItemsList",
	"ItemIcon",
	"ItemCover",
	"ItemText",
	"HintText",
	"LoadingIcon",
};

// AttributeText ////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void drawAttributeText(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (curItem) {
		if (curItem->item.id != -1) {
			if (!curItem->item.config) {
				item_list_t *support = curMenu->item->userdata;
				curItem->item.config = support->itemGetConfig(curItem->item.id);
			}

			char *temp;
			if (configGetStr(curItem->item.config, (char *) elem->extended, &temp))
				fntRenderString(elem->font, elem->posX, elem->posY, elem->aligned, temp, elem->color);
		}
	}
}

static void initAttributeText(char* themePath, config_set_t* themeConfig, theme_t* theme, theme_element_t* elem, char* name,
		char* attribute) {

	char elemProp[64];

	snprintf(elemProp, 64, "%s_attribute", name);
	configGetStr(themeConfig, elemProp, &attribute);
	if (attribute) {
		LOG("elemAttributeText %s: got an attribute: %s\n", name, attribute);

		int length = strlen(attribute) + 1;
		elem->extended = (char*) malloc(length * sizeof(char));
		memcpy(elem->extended, attribute, length);

		elem->drawElem = &drawAttributeText;
	} else
		LOG("elemAttributeText %s: NO s_attribute, elem disabled !!\n", name);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void freeOverlayImage(image_overlay_t* overlay) {
	if (overlay) {
		if (overlay->texture.Mem)
			free(overlay->texture.Mem);
		free(overlay);
	}
}

static image_overlay_t* checkOverlayImage(char* themePath, config_set_t* themeConfig, char* name) {
	int intValue;
	char* overlayTex;
	char elemProp[64];

	image_overlay_t* overlay = NULL;
	snprintf(elemProp, 64, "%s_overlay", name);
	if (configGetStr(themeConfig, elemProp, &overlayTex)) {
		char path[255];
		snprintf(path, 255, "%s%s", themePath, overlayTex);
		overlay = (image_overlay_t*) malloc(sizeof(image_overlay_t));
		texPrepare(&overlay->texture, GS_PSM_CT32);
		if (texDiscoverLoad(&overlay->texture, path, -1, GS_PSM_CT32) >= 0) {
			snprintf(elemProp, 64, "%s_overlay_ulx", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->upperLeft_x = intValue;
			snprintf(elemProp, 64, "%s_overlay_uly", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->upperLeft_y = intValue;
			snprintf(elemProp, 64, "%s_overlay_urx", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->upperRight_x = intValue;
			snprintf(elemProp, 64, "%s_overlay_ury", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->upperRight_y = intValue;
			snprintf(elemProp, 64, "%s_overlay_llx", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->lowerLeft_x = intValue;
			snprintf(elemProp, 64, "%s_overlay_lly", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->lowerLeft_y = intValue;
			snprintf(elemProp, 64, "%s_overlay_lrx", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->lowerRight_x = intValue;
			snprintf(elemProp, 64, "%s_overlay_lry", name);
			if (configGetInt(themeConfig, elemProp, &intValue))
				overlay->lowerRight_y = intValue;
		} else {
			freeOverlayImage(overlay);
			overlay = NULL;
		}
	}
	return overlay;
}

// AttributeImage ///////////////////////////////////////////////////////////////////////////////////////////////////////////

/*static void drawAttributeImage(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (curItem) {
		if (curItem->item.id != -1) {
			if (!curItem->item.config) {
				item_list_t *support = curMenu->item->userdata;
				curItem->item.config = support->itemGetConfig(curItem->item.id);
			}

			attribute_image_t* attributeImage = (attribute_image_t*) elem->extended;

			char *temp;
			if (configGetStr(curItem->item.config, attributeImage->attribute, &temp)) {
				if (attributeImage->overlay) {
					image_overlay_t* overlay = (image_overlay_t*) attributeImage->overlay;
					GSTEXTURE* inlayTex = getGameCachedTex(attributeImage->cache, curMenu->item->userdata, &curItem->item, gameImage->pattern, &attributeImage->defaultTex);
					if (inlayTex && inlayTex->Mem) {
						rmDrawOverlayPixmap(&overlay->texture, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol,
								inlayTex, overlay->upperLeft_x, overlay->upperLeft_y, overlay->upperRight_x, overlay->upperRight_y,
							overlay->lowerLeft_x, overlay->lowerLeft_y, overlay->lowerRight_x, overlay->lowerRight_y);
					}
				} else {
					GSTEXTURE* imageTex = getGameCachedTex(attributeImage->cache, curMenu->item->userdata, &curItem->item, gameImage->pattern, &attributeImage->defaultTex);
					if (imageTex && imageTex->Mem)
						rmDrawPixmap(imageTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol);
				}
			}
		}
	}
}

static void endAttributeImage(struct theme_element* elem) {
	char* attribute = (char*) elem->extended;
	free(attribute);

	endBasic(elem);
}

static theme_element_t* initAttributeImage(config_set_t* themeConfig, theme_t* theme, theme_element_t* elem, char* name) {

}*/

// StaticImage //////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void drawStaticImage(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (elem->extended) {
		if (curItem) {
			static_image_t* staticImage = (static_image_t*) elem->extended;
			if (staticImage->texture.Mem) {
				if (staticImage->overlay) {
					image_overlay_t* overlay = (image_overlay_t*) staticImage->overlay;
					rmDrawOverlayPixmap(&overlay->texture, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol,
							&staticImage->texture, overlay->upperLeft_x, overlay->upperLeft_y, overlay->upperRight_x, overlay->upperRight_y,
							overlay->lowerLeft_x, overlay->lowerLeft_y, overlay->lowerRight_x, overlay->lowerRight_y);
				} else
					rmDrawPixmap(&staticImage->texture, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol);
			}
		}
	} else
		// little tweak, StaticImage (without an extended field) is used to draw the Plasma
		guiDrawBGPlasma();
}

static void endStaticImage(struct theme_element* elem) {
	static_image_t* staticImage = (static_image_t*) elem->extended;

	if (staticImage->texture.Mem)
		free(staticImage->texture.Mem);

	freeOverlayImage(staticImage->overlay);

	free(staticImage);

	free(elem);
}

static void initStaticImage(char* themePath, config_set_t* themeConfig, theme_t* theme, theme_element_t* elem, char* name,
		char* imageName) {

	char elemProp[64];

	snprintf(elemProp, 64, "%s_name", name);
	configGetStr(themeConfig, elemProp, &imageName);
	if (imageName) {
		LOG("elemStaticImage %s: got an image name: %s\n", name, imageName);

		static_image_t* staticImage = (static_image_t*) malloc(sizeof(static_image_t));
		texPrepare(&staticImage->texture, GS_PSM_CT24);
		char path[255];
		snprintf(path, 255, "%s%s", themePath, imageName);
		texDiscoverLoad(&staticImage->texture, path, -1, GS_PSM_CT24);

		staticImage->overlay = checkOverlayImage(themePath, themeConfig, name);

		elem->extended = staticImage;

		elem->drawElem = &drawStaticImage;
		elem->endElem = &endStaticImage;
	} else
		LOG("elemStaticImage %s: NO image name, elem disabled !!\n", name);
}

// GameImage ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static GSTEXTURE* getGameImageTexture(image_cache_t* cache, void* support, submenu_item_t* item, char* suffix, GSTEXTURE* dflt) {
	if (gEnableArt && (item->id != -1)) {
		item_list_t * list = (item_list_t *) support;
		char* startup = list->itemGetStartup(item->id);
		//LOG("getGameCachedTex, prefix: %s addsep: %d value: %s suffix: %s\n", cache->prefix, cache->addSeparator, startup, suffix);
		GSTEXTURE* cacheTxt = cacheGetTexture(cache, list, &item->cache_id[cache->userId], &item->cache_uid[cache->userId], startup, suffix);
		if (cacheTxt && cacheTxt->Mem)  // but did it produce a valid entry?
			return cacheTxt;
	}

	return dflt;
}

static void drawGameImage(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (curItem) {
		game_image_t* gameImage = (game_image_t*) elem->extended;
		GSTEXTURE* imageTex = getGameImageTexture(gameImage->cache, curMenu->item->userdata, &curItem->item, gameImage->pattern, &gameImage->defaultTex);
		if (imageTex && imageTex->Mem) {
			if (gameImage->overlay) {
				image_overlay_t* overlay = (image_overlay_t*) gameImage->overlay;
				rmDrawOverlayPixmap(&overlay->texture, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol,
						imageTex, overlay->upperLeft_x, overlay->upperLeft_y, overlay->upperRight_x, overlay->upperRight_y,
						overlay->lowerLeft_x, overlay->lowerLeft_y, overlay->lowerRight_x, overlay->lowerRight_y);
			} else
				rmDrawPixmap(imageTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol);
			return;
		}
	}
	if (elem->type == TYPE_BACKGROUND)
		guiDrawBGPlasma();
}

static void endGameImage(struct theme_element* elem) {
	game_image_t* gameImage = (game_image_t*) elem->extended;

	cacheDestroyCache(gameImage->cache);

	if (gameImage->defaultTex.Mem)
		free(gameImage->defaultTex.Mem);

	free(gameImage->pattern);

	freeOverlayImage(gameImage->overlay);

	free(gameImage);

	free(elem);
}

static void initGameImage(char* themePath, config_set_t* themeConfig, theme_t* theme, theme_element_t* elem, char* name,
		char* pattern, char* defaultTex, int count) {

	char elemProp[64];

	snprintf(elemProp, 64, "%s_pattern", name);
	configGetStr(themeConfig, elemProp, &pattern);
	if (pattern) {
		LOG("elemGameImage %s: got a pattern: %s\n", name, pattern);

		game_image_t* gameImage = (game_image_t*) malloc(sizeof(game_image_t));
		int length = strlen(pattern) + 1;
		gameImage->pattern = (char*) malloc(length * sizeof(char));
		memcpy(gameImage->pattern, pattern, length);

		texPrepare(&gameImage->defaultTex, GS_PSM_CT24);
		snprintf(elemProp, 64, "%s_default", name);
		configGetStr(themeConfig, elemProp, &defaultTex);
		if (defaultTex) {
			LOG("elemGameImage %s: got a default: %s\n", name, defaultTex);
			char path[255];
			snprintf(path, 255, "%s%s", themePath, defaultTex);
			texDiscoverLoad(&gameImage->defaultTex, path, -1, GS_PSM_CT24);
			LOG("elemGameImage %s: is texture loaded valid ? %x\n", name, gameImage->defaultTex.Mem);
		}

		snprintf(elemProp, 64, "%s_count", name);
		configGetInt(themeConfig, elemProp, &count);
		LOG("elemGameImage %s: using cache size of: %d\n", name, count);

		gameImage->cache = cacheInitCache(theme->gameCacheCount++, "ART", 1, count);
		gameImage->overlay = checkOverlayImage(themePath, themeConfig, name);

		elem->extended = gameImage;

		elem->drawElem = &drawGameImage;
		elem->endElem = &endGameImage;
	} else
		LOG("elemGameImage %s: NO pattern, elem disabled !!\n", name);
}

// BasicElement /////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void endBasic(theme_element_t* elem) {
	if (elem->extended)
		free(elem->extended);

	free(elem);
}

static theme_element_t* initBasic(char* themePath, config_set_t* themeConfig, theme_t* theme, int screenWidth, int screenHeight,
		char* name,	int type, int x, int y, short aligned, int w, int h, u64 color, int font) {

	int intValue;
	unsigned char charColor[3];
	char* temp;
	char elemProp[64];

	theme_element_t* elem = (theme_element_t*) malloc(sizeof(theme_element_t));

	elem->type = type;
	elem->extended = NULL;
	elem->drawElem = NULL;
	elem->endElem = &endBasic;
	elem->next = NULL;

	snprintf(elemProp, 64, "%s_x", name);
	if (configGetStr(themeConfig, elemProp, &temp)) {
		if (!strncmp(temp, "POS_MID", 7))
			x = screenWidth >> 1;
		else
			x = atoi(temp);
	}
	if (x < 0)
		elem->posX = screenWidth + x;
	else
		elem->posX = x;

	snprintf(elemProp, 64, "%s_y", name);
	if (configGetStr(themeConfig, elemProp, &temp)) {
		if (!strncmp(temp, "POS_MID", 7))
			y = screenHeight >> 1;
		else
			y = atoi(temp);
	}
	if (y < 0)
		elem->posY = ceil((screenHeight + y) * theme->usedHeight / screenHeight);
	else
		elem->posY = y;

	snprintf(elemProp, 64, "%s_width", name);
	if (configGetInt(themeConfig, elemProp, &intValue))
		elem->width = intValue;
	else
		elem->width = w;

	snprintf(elemProp, 64, "%s_height", name);
	if (configGetInt(themeConfig, elemProp, &intValue))
		elem->height = intValue;
	else
		elem->height = h;

	snprintf(elemProp, 64, "%s_aligned", name);
	if (configGetInt(themeConfig, elemProp, &intValue))
		elem->aligned = intValue;
	else
		elem->aligned = aligned;
	if (elem->aligned) { // DIM_INF and aligned are incompatible
		if ((elem->width == DIM_INF) || (elem->height == DIM_INF))
			elem->aligned = ALIGN_NONE;
	}

	snprintf(elemProp, 64, "%s_color", name);
	if (configGetColor(themeConfig, elemProp, charColor))
		elem->color = GS_SETREG_RGBA(charColor[0], charColor[1], charColor[2], 0xff);
	else
		elem->color = color;

	snprintf(elemProp, 64, "%s_font", name);
	if (configGetInt(themeConfig, elemProp, &intValue))
		font = intValue;
	if (font >= 0 && font < THM_MAX_FONTS)
		elem->font = theme->fonts[font];
	else
		elem->font = FNT_DEFAULT;

	return elem;
}

// Internal elements ////////////////////////////////////////////////////////////////////////////////////////////////////////

static void drawMenuIcon(struct menu_list *curMenu, struct submenu_list *curItem, struct theme_element* elem) {
	GSTEXTURE* menuIconTex = thmGetTexture(curMenu->item->icon_id);
	if (menuIconTex && menuIconTex->Mem)
		rmDrawPixmap(menuIconTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, gDefaultCol);
}

static void drawMenuText(struct menu_list *curMenu, struct submenu_list *curItem, struct theme_element* elem) {
	GSTEXTURE* leftIconTex = NULL, *rightIconTex = NULL;
	if (curMenu->prev)
		leftIconTex = thmGetTexture(LEFT_ICON);
	if (curMenu->next)
		rightIconTex = thmGetTexture(RIGHT_ICON);

	if (elem->aligned) {
		int offset = elem->width >> 1;
		if (leftIconTex && leftIconTex->Mem)
			rmDrawPixmap(leftIconTex, elem->posX - offset, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		if (rightIconTex && rightIconTex->Mem)
			rmDrawPixmap(rightIconTex, elem->posX + offset, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
	}
	else {
		if (leftIconTex && leftIconTex->Mem)
			rmDrawPixmap(leftIconTex, elem->posX - leftIconTex->Width, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		if (rightIconTex && rightIconTex->Mem)
			rmDrawPixmap(rightIconTex, elem->posX + elem->width, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
	}
	fntRenderString(elem->font, elem->posX, elem->posY, elem->aligned, menuItemGetText(curMenu->item), elem->color);
}

static void drawItemsList(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (curItem) {
		items_list_t* itemsList = (items_list_t*) elem->extended;
		int icnt = itemsList->displayedItems;
		int found = 0;
		submenu_list_t *ps  = curMenu->item->pagestart;

		// verify the item is in visible range
		while (icnt-- && ps) {
			if (ps == curItem) {
				found = 1;
				break;
			}
			ps = ps->next;
		}

		// page not properly aligned?
		if (!found)
			curMenu->item->pagestart = curItem;

		// reset to page start after cur. item visibility determination
		ps  = curMenu->item->pagestart;

		game_image_t* gameImage = (game_image_t*) itemsList->decoratorImage;
		GSTEXTURE* itemIconTex = NULL;
		int stretchedSize = 0;
		if (gameImage)
			stretchedSize = 20;

		int curpos = elem->posY;
		int others = 0;
		u64 color;
		while (ps && (others < itemsList->displayedItems)) {
			if (gameImage) {
				itemIconTex = getGameImageTexture(gameImage->cache, curMenu->item->userdata, &ps->item, gameImage->pattern, &gameImage->defaultTex);
				if (itemIconTex && itemIconTex->Mem)
					rmDrawPixmap(itemIconTex, elem->posX, curpos + others * MENU_ITEM_HEIGHT, ALIGN_NONE, stretchedSize, stretchedSize, gDefaultCol);
			}

			if (ps == curItem)
				color = gTheme->selTextColor;
			else
				color = elem->color;

			fntRenderString(elem->font, elem->posX + stretchedSize, curpos + others * MENU_ITEM_HEIGHT, ALIGN_NONE, submenuItemGetText(&ps->item), color);

			ps = ps->next;
			others++;
		}
	}
}

static void initItemsList(char* themePath, config_set_t* themeConfig, theme_t* theme, theme_element_t* elem, char* name,
		int displayedItems, char* decorator) {

	int intValue;
	char elemProp[64];

	items_list_t* itemsList = (items_list_t*) malloc(sizeof(items_list_t));

	snprintf(elemProp, 64, "%s_items", name);
	if (configGetInt(themeConfig, elemProp, &intValue)) {
		if (intValue < displayedItems)
			displayedItems = intValue;
	}
	itemsList->displayedItems = displayedItems;
	elem->height = displayedItems * MENU_ITEM_HEIGHT;
	LOG("elemItemsList %s: displaying %d elems, item height: %d\n", name, itemsList->displayedItems, elem->height);

	itemsList->decorator = NULL;
	snprintf(elemProp, 64, "%s_decorator", name);
	configGetStr(themeConfig, elemProp, &decorator);
	if (decorator) {
		LOG("elemItemsList %s: got a decorator: %s\n", name, decorator);

		// Will be used later (thmValidate)
		itemsList->decorator = decorator;
	}
	itemsList->decoratorImage = NULL;

	elem->extended = itemsList;

	elem->drawElem = &drawItemsList;
}

static void drawItemText(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	if (curItem) {
		if (curItem->item.id != -1) {
			item_list_t *support = curMenu->item->userdata;
			fntRenderString(elem->font, elem->posX, elem->posY, elem->aligned, support->itemGetStartup(curItem->item.id), elem->color);
		}
	}
}

static void drawHintText(struct menu_list* curMenu, struct submenu_list* curItem, struct theme_element* elem) {
	menu_hint_item_t* hint = curMenu->item->hints;
	if (hint) {
		GSTEXTURE* hintIconTex = NULL;
		int x = elem->posX;
		int y = elem->posY;

		for (; hint; hint = hint->next) {
			hintIconTex = thmGetTexture(hint->icon_id);
			if (hintIconTex && hintIconTex->Mem) {
				rmDrawPixmap(hintIconTex, x, y, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
				x += hintIconTex->Width + 2;
			}

			x += fntRenderString(elem->font, x, y, ALIGN_NONE, _l(hint->text_id), elem->color);
			x += 12;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void thmValidate(char* themePath, config_set_t* themeConfig, theme_t* theme, int screenWidth, int screenHeight) {
	// 1. check we have a valid Background element
	if ( !theme->elems || (theme->elems->type != TYPE_BACKGROUND) ) {
		LOG("No valid background found, add BG_ART\n");
		// Add a default Plasma
		theme_element_t* backgroundElem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, "bg", TYPE_BACKGROUND, 0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gDefaultCol, FNT_DEFAULT);
		if (themePath)
			initGameImage(themePath, themeConfig, theme, backgroundElem, "bg", "BG", "background", 5);
		else
			initGameImage(themePath, themeConfig, theme, backgroundElem, "bg", "BG", NULL, 5);
		backgroundElem->next = theme->elems;
		theme->elems = backgroundElem;
	}

	// 2. check we have a valid ItemsList element, and link its decorator to the target element
	// First pass to find the item list
	theme_element_t* itemsListElem = theme->elems;
	while (itemsListElem) {
		if (itemsListElem->type == TYPE_ITEMS_LIST)
			break;

		itemsListElem = itemsListElem->next;
	}

	if (itemsListElem) {
		items_list_t* itemsList = (items_list_t*) itemsListElem->extended;
		if (itemsList->decorator) {
			// Second pass to find the decorator
			theme_element_t* decoratorElem = theme->elems;
			while (decoratorElem) {
				if (decoratorElem->type == TYPE_GAME_IMAGE) {
					game_image_t* gameImage = (game_image_t*) decoratorElem->extended;
					if (!strcmp(itemsList->decorator, gameImage->pattern)) {
						// if user want to cache less than displayed items, then disable itemslist icons, if not would load constantly
						if (gameImage->cache->count >= itemsList->displayedItems)
							itemsList->decoratorImage = gameImage;
						break;
					}
				}

				decoratorElem = decoratorElem->next;
			}
		}
	} else {
		itemsListElem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, "il", TYPE_ITEMS_LIST, 150, MENU_POS_V, ALIGN_NONE, DIM_INF, 0, theme->textColor, FNT_DEFAULT);
		initItemsList(themePath, themeConfig, theme, itemsListElem, "il", (theme->usedHeight - (MENU_POS_V + HINT_HEIGHT)) / MENU_ITEM_HEIGHT, NULL);
		theme->itemsList = itemsListElem;
		itemsListElem->next = theme->elems->next;
		theme->elems->next = itemsListElem;
	}

}

static theme_element_t* thmAddElement(char* themePath, config_set_t* themeConfig, theme_t* theme, theme_element_t* previous,
		char* type, int screenWidth, int screenHeight, char* name) {

	int enabled = 1;
	char elemProp[64];
	theme_element_t* elem;

	snprintf(elemProp, 64, "%s_enabled", name);
	configGetInt(themeConfig, elemProp, &enabled);

	if (enabled) {
		snprintf(elemProp, 64, "%s_type", name);
		configGetStr(themeConfig, elemProp, &type);
		if (type) {
			if (!strcmp(elementsType[TYPE_ATTRIBUTE_TEXT], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_ATTRIBUTE_TEXT, 0, 0, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				initAttributeText(themePath, themeConfig, theme, elem, name, NULL);
			} else if (!strcmp(elementsType[TYPE_ATTRIBUTE_IMAGE], type)) {
				return previous;
			} else if (!strcmp(elementsType[TYPE_GAME_IMAGE], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_GAME_IMAGE, 0, 0, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				initGameImage(themePath, themeConfig, theme, elem, name, NULL, NULL, 1);
			} else if (!strcmp(elementsType[TYPE_STATIC_IMAGE], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_STATIC_IMAGE, 0, 0, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				initStaticImage(themePath, themeConfig, theme, elem, name, NULL);
			} else if (!strcmp(elementsType[TYPE_BACKGROUND], type)) {
				if (previous) { // Background elem can only be the first one, so discard here
					LOG("elem %s: Invalid Background type not a first place !!\n", name);
					return previous;
				}
				else {
					elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_BACKGROUND, 0, 0, ALIGN_NONE, DIM_INF, DIM_INF, gDefaultCol, FNT_DEFAULT);
					// First try to discover a BG_ART background
					initGameImage(themePath, themeConfig, theme, elem, name, NULL, "background", 5);
					if (!elem->drawElem) {
						// Next try to discover a BG_PICTURE background
						initStaticImage(themePath, themeConfig, theme, elem, name, NULL);
						if (!elem->drawElem) {
							// Finally set BG_PLASMA
							elem->drawElem = &drawStaticImage;
						}
					}
				}
			} else if (!strcmp(elementsType[TYPE_MENU_ICON], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_MENU_ICON, 40, 40, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				elem->drawElem = &drawMenuIcon;
			} else if (!strcmp(elementsType[TYPE_MENU_TEXT], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_MENU_TEXT, screenWidth >> 1, 20, ALIGN_CENTER, 200, 20, theme->textColor, FNT_DEFAULT);
				elem->drawElem = &drawMenuText;
			} else if (!strcmp(elementsType[TYPE_ITEMS_LIST], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_ITEMS_LIST, 150, MENU_POS_V, ALIGN_NONE, DIM_INF, 0, theme->textColor, FNT_DEFAULT);
				initItemsList(themePath, themeConfig, theme, elem, name, (theme->usedHeight - (MENU_POS_V + HINT_HEIGHT)) / MENU_ITEM_HEIGHT, NULL);
				theme->itemsList = elem;
			} else if (!strcmp(elementsType[TYPE_ITEM_ICON], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_GAME_IMAGE, 80, theme->usedHeight >> 1, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				initGameImage(themePath, themeConfig, theme, elem, name, "ICO", NULL, 30);
			} else if (!strcmp(elementsType[TYPE_ITEM_COVER], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_GAME_IMAGE, 520, theme->usedHeight >> 1, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				initGameImage(themePath, themeConfig, theme, elem, name, "COV", NULL, 10);
			} else if (!strcmp(elementsType[TYPE_ITEM_TEXT], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_ITEM_TEXT, 520, 370, ALIGN_CENTER, DIM_UNDEF, 20, theme->textColor, FNT_DEFAULT);
				elem->drawElem = &drawItemText;
			} else if (!strcmp(elementsType[TYPE_HINT_TEXT], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_HINT_TEXT, 16, -HINT_HEIGHT, ALIGN_NONE, DIM_INF, HINT_HEIGHT, theme->textColor, FNT_DEFAULT);
				elem->drawElem = &drawHintText;
			} else if (!strcmp(elementsType[TYPE_LOADING_ICON], type)) {
				elem = initBasic(themePath, themeConfig, theme, screenWidth, screenHeight, name, TYPE_LOADING_ICON, -50, -50, ALIGN_CENTER, DIM_UNDEF, DIM_UNDEF, gDefaultCol, FNT_DEFAULT);
				theme->loadingIcon = elem;
				// no drawElem set, so will be added to the list of elems but not drawn
			} else {
				LOG("elem %s: Invalid element type: %s\n", name, type);
				return previous;
			}
			LOG("elem %s: of type: %s is valid\n", name, type);
			if (previous)
				previous->next = elem;
			else
				theme->elems = elem;
			return elem;
		} else
			return NULL; // ends the reading of elements
	}
	return previous;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
		// free elements
		theme_element_t* elem = theme->elems;
		while (elem) {
			theme->elems = elem->next;
			elem->endElem(elem);
			elem = theme->elems;
		}

		// free textures
		GSTEXTURE* texture;
		int id = 0;
		for(; id < TEXTURES_COUNT; id++) {
			texture = &theme->textures[id];
			if (texture->Mem != NULL) {
				free(texture->Mem);
				texture->Mem = NULL;
			}
		}

		// free fonts
		for (id = 0; id < THM_MAX_FONTS; ++id) {
			int fntid = theme->fonts[id];

			if (fntid != FNT_DEFAULT)
				fntRelease(fntid);
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

		length = strlen(path) + 1 + strlen(name) + 1 + 1;
		currTheme->filePath = (char*) malloc(length * sizeof(char));
		sprintf(currTheme->filePath, "%s%s%s%s", path, separator, name, separator);

		LOG("Theme found: %s\n", currTheme->filePath);

		index++;
	}
	return index;
}

/* themePath must contains the leading separator (as it is dependent of the device, we can't know here) */
static int thmLoadResource(int texId, char* themePath, short psm, int useDefault) {
	int success = -1;
	GSTEXTURE* texture = &gTheme->textures[texId];

	if (themePath != NULL)
		success = texDiscoverLoad(texture, themePath, texId, psm); // only set success here

	if ((success < 0) && useDefault)
		texPngLoad(texture, NULL, texId, psm);  // we don't mind the result of "default"

	return success;
}

static void thmSetColors(theme_t* theme) {
	memcpy(theme->bgColor, gDefaultBgColor, 3);
	theme->textColor = GS_SETREG_RGBA(gDefaultTextColor[0], gDefaultTextColor[1], gDefaultTextColor[2], 0xff);
	theme->uiTextColor = GS_SETREG_RGBA(gDefaultUITextColor[0], gDefaultUITextColor[1], gDefaultUITextColor[2], 0xff);
	theme->selTextColor = GS_SETREG_RGBA(gDefaultSelTextColor[0], gDefaultSelTextColor[1], gDefaultSelTextColor[2], 0xff);

	theme_element_t* elem = theme->elems;
	while (elem) {
		elem->color = theme->textColor;
		elem = elem->next;
	}
}

static void thmLoadFonts(config_set_t* themeConfig, const char* themePath, theme_t* theme) {
    int fntID; // theme side font id, not the fntSys handle
	for (fntID = -1; fntID < THM_MAX_FONTS; ++fntID) {
		// does the font by the key exist?
		char fntKey[16];

		// -1 is a placeholder for default font...
		if (fntID >= 0) {
			// Default font handle...
			theme->fonts[fntID] = FNT_DEFAULT;
			snprintf(fntKey, 16, "font%d", fntID);
		} else {
			snprintf(fntKey, 16, "default_font");
		}

		char *fntFile;
		int cfgKeyOK = configGetStr(themeConfig, fntKey, &fntFile);
		if (!cfgKeyOK && (fntID >= 0))
			continue;

		char fullPath[128];

		if (fntID < 0) {
			// replace the default font
			if (cfgKeyOK) {
				snprintf(fullPath, 128, "%s%s", themePath, fntFile);

				int size = -1;
				void* customFont = readFile(fullPath, -1, &size);

				if (customFont)
					fntReplace(FNT_DEFAULT, customFont, size, 1, 0);
			} else
				fntSetDefault(FNT_DEFAULT);
		} else {
			snprintf(fullPath, 128, "%s%s", themePath, fntFile);
			int fntHandle = fntLoadFile(fullPath);

			// Do we have a valid font? Assign the font handle to the theme font slot
			if (fntHandle != FNT_ERROR)
				theme->fonts[fntID] = fntHandle;
		};
	};
}

static void thmLoad(char* themePath) {
	LOG("thmLoad() path=%s\n", themePath);
	theme_t* curT = gTheme;
	theme_t* newT = (theme_t*) malloc(sizeof(theme_t));
	memset(newT, 0, sizeof(theme_t));

	newT->useDefault = 1;
	newT->usedHeight = 480;
	thmSetColors(newT);
	newT->elems = NULL;
	newT->gameCacheCount = 0;
	newT->itemsList = NULL;
	newT->loadingIcon = NULL;
	newT->loadingIconCount = LOAD7_ICON - LOAD0_ICON + 1;

	config_set_t* themeConfig = NULL;
	if (!themePath) {
		themeConfig = configAlloc(0, NULL, NULL);
		theme_element_t* elem = NULL;
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_MENU_ICON], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_MENU_TEXT], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_ITEMS_LIST], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_ITEM_ICON], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_ITEM_COVER], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_ITEM_TEXT], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_HINT_TEXT], screenWidth, screenHeight, "_");
		elem = thmAddElement(themePath, themeConfig, newT, elem, elementsType[TYPE_LOADING_ICON], screenWidth, screenHeight, "_");

		// reset the default font to be sure
		fntSetDefault(FNT_DEFAULT);
	} else {
		char path[255];
		snprintf(path, 255, "%sconf_theme.cfg", themePath);
		themeConfig = configAlloc(0, NULL, path);
		configRead(themeConfig); // try to load the theme config file

		int intValue;
		if (configGetInt(themeConfig, "use_default", &intValue))
			newT->useDefault = intValue;

		if (configGetInt(themeConfig, "use_real_height", &intValue)) {
			if (intValue)
				newT->usedHeight = screenHeight;
		}

		configGetColor(themeConfig, "bg_color", newT->bgColor);

		unsigned char color[3];
		if (configGetColor(themeConfig, "text_color", color))
			newT->textColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

		if (configGetColor(themeConfig, "ui_text_color", color))
			newT->uiTextColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

		if (configGetColor(themeConfig, "sel_text_color", color))
			newT->selTextColor = GS_SETREG_RGBA(color[0], color[1], color[2], 0xff);

		// before loading the element definitions, we have to have the fonts prepared
		// for that, we load the fonts and a translation table
		thmLoadFonts(themeConfig, themePath, newT);

		int i = 1;
		theme_element_t* elem = NULL;
		snprintf(path, 255, "main0");
		do {
			elem = thmAddElement(themePath, themeConfig, newT, elem, NULL, screenWidth, screenHeight, path);
			snprintf(path, 255, "main%d", i++);
		} while (elem);
	}

	thmValidate(themePath, themeConfig, newT, screenWidth, screenHeight);
	configFree(themeConfig);

	LOG("theme loaded, number of cache: %d\n", newT->gameCacheCount);

	LOG("thmLoad() usedHeight=%d\n", newT->usedHeight);

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
		if (thmLoadResource(i, path, GS_PSM_CT32, gTheme->useDefault) >= 0)
			customBusy = 1;
		else {
			if (customBusy)
				break;
			else
				path = NULL;
		}
	}
	gTheme->loadingIconCount = i;

	// Customizable icons
	for (i = USB_ICON; i <= DOWN_ICON; i++)
		thmLoadResource(i, themePath, GS_PSM_CT32, gTheme->useDefault);

	// Not  customizable icons
	for (i = L1_ICON; i <= START_ICON; i++)
		thmLoadResource(i, NULL, GS_PSM_CT32, 1);

	// LOGO is hardcoded
	thmLoadResource(LOGO_PICTURE, NULL, GS_PSM_CT24, 1);
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
	nThemes += listDir(path, separator, THM_MAX_FILES - nThemes, &thmReadEntry);
	LOG("thmAddElements() nThemes=%d\n", nThemes);
	thmRebuildGuiNames();

	char* temp;
	if (configGetStr(configGetByType(CONFIG_OPL), "theme", &temp)) {
		LOG("Trying to set again theme: %s\n", temp);
		thmSetGuiValue(thmFindGuiID(temp), 0);
	}
}

void thmInit() {
	LOG("thmInit()\n");
	gTheme = NULL;

	thmReloadScreenExtents();

	// initialize default internal
	thmLoad(NULL);

	thmAddElements(gBaseMCDir, "/");
}

void thmReloadScreenExtents() {
	rmGetScreenExtents(&screenWidth, &screenHeight);
}

char* thmGetValue() {
	//LOG("thmGetValue() id=%d name=%s\n", guiThemeID, guiThemesNames[guiThemeID]);
	return guiThemesNames[guiThemeID];
}

void thmSetGuiValue(int themeID, int reload) {
	LOG("thmSetGuiValue() id=%d\n", themeID);
	if (guiThemeID != themeID || reload) {
		// negative theme id means reload
		if (themeID < 0)
			themeID = guiThemeID;

		if (themeID != 0)
			thmLoad(themes[themeID - 1].filePath);
		else
			thmLoad(NULL);
		guiThemeID = themeID;
		configSetStr(configGetByType(CONFIG_OPL), "theme", thmGetValue());
	}
	else if (guiThemeID == 0)
		thmSetColors(gTheme);
}

int thmGetGuiValue() {
	//LOG("thmGetGuiValue() id=%d\n", guiThemeID);
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
