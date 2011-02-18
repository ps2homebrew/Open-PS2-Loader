/*
  Copyright 2009, Ifcaro & volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/menusys.h"
#include "include/iosupport.h"
#include "include/usbld.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/lang.h"
#include "include/themes.h"
#include "include/pad.h"
#include "include/gui.h"
#include "include/system.h"
#include "include/ioman.h"
#include "assert.h"

#define MENU_SETTINGS		0
#define MENU_GFX_SETTINGS	1
#define MENU_IP_CONFIG		2
#define MENU_SAVE_CHANGES	3
#define MENU_START_HDL		4
#define MENU_ABOUT			5
#define MENU_EXIT			6
#define MENU_POWER_OFF		7

// global menu variables
static menu_list_t* menu;
static menu_list_t* selected_item;

static int itemIdConfig;
static config_set_t* itemConfig;

// "main menu submenu"
static submenu_list_t* mainMenu;
// active item in the main menu
static submenu_list_t* mainMenuCurrent;

static void menuInitMainMenu() {
	if (mainMenu)
		submenuDestroy(&mainMenu);

	// initialize the menu
#ifndef __CHILDPROOF
	submenuAppendItem(&mainMenu, -1, NULL, MENU_SETTINGS, _STR_SETTINGS);
	submenuAppendItem(&mainMenu, -1, NULL, MENU_GFX_SETTINGS, _STR_GFX_SETTINGS);
	submenuAppendItem(&mainMenu, -1, NULL, MENU_IP_CONFIG, _STR_IPCONFIG);
	submenuAppendItem(&mainMenu, -1, NULL, MENU_SAVE_CHANGES, _STR_SAVE_CHANGES);
	if (gHDDStartMode) // enabled at all?
		submenuAppendItem(&mainMenu, -1, NULL, MENU_START_HDL, _STR_STARTHDL);
#endif
	submenuAppendItem(&mainMenu, -1, NULL, MENU_ABOUT, _STR_ABOUT);
	submenuAppendItem(&mainMenu, -1, NULL, MENU_EXIT, _STR_EXIT);
	submenuAppendItem(&mainMenu, -1, NULL, MENU_POWER_OFF, _STR_POWEROFF);

	mainMenuCurrent = mainMenu;
}

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit() {
	menu = NULL;
	selected_item = NULL;
	itemIdConfig = -1;
	itemConfig = NULL;
	mainMenu = NULL;
	mainMenuCurrent = NULL;
	menuInitMainMenu();
}

void menuEnd() {
	// destroy menu
	menu_list_t *cur = menu;
	
	while (cur) {
		menu_list_t *td = cur;
		cur = cur->next;
		
		if (&td->item)
			submenuDestroy(&td->item->submenu);
		
		menuRemoveHints(td->item);
		
		free(td);
	}

	submenuDestroy(&mainMenu);

	if (itemConfig)
		configFree(itemConfig);
}

static menu_list_t* AllocMenuItem(menu_item_t* item) {
	menu_list_t* it;
	
	it = malloc(sizeof(menu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item = item;
	
	return it;
}

void menuAppendItem(menu_item_t* item) {
	assert(item);
	
	if (menu == NULL) {
		menu = AllocMenuItem(item);
		return;
	}
	
	menu_list_t *cur = menu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	menu_list_t *newitem = AllocMenuItem(item);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
}


static submenu_list_t* AllocSubMenuItem(int icon_id, char *text, int id, int text_id) {
	submenu_list_t* it;
	
	it = malloc(sizeof(submenu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item.icon_id = icon_id;
	it->item.text = text;
	it->item.text_id = text_id;
	it->item.id = id;
	
	// no cache entry yet
	int size = gTheme->gameCacheCount * sizeof(int);
	it->item.cache_id = malloc(size);
	memset(it->item.cache_id, -1, size);
	it->item.cache_uid = malloc(size);
	memset(it->item.cache_uid, -1, size);
	
	return it;
}

submenu_list_t* submenuAppendItem(submenu_list_t** submenu, int icon_id, char *text, int id, int text_id) {
	if (*submenu == NULL) {
		*submenu = AllocSubMenuItem(icon_id, text, id, text_id);
		return *submenu; 
	}
	
	submenu_list_t *cur = *submenu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	submenu_list_t *newitem = AllocSubMenuItem(icon_id, text, id, text_id);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
	
	return newitem;
}

static void submenuDestroyItem(submenu_list_t* submenu) {
	free(submenu->item.cache_id);
	free(submenu->item.cache_uid);

	free(submenu);
}

void submenuRemoveItem(submenu_list_t** submenu, int id) {
	submenu_list_t* cur = *submenu;
	submenu_list_t* prev = NULL;
	
	while (cur) {
		if (cur->item.id == id) {
			submenu_list_t* next = cur->next;
			
			if (prev)
				prev->next = cur->next;
			
			if (*submenu == cur)
				*submenu = next;
			
			submenuDestroyItem(cur);
			
			cur = next;
		} else {
			prev = cur;
			cur = cur->next;
		}
	}
}

void submenuDestroy(submenu_list_t** submenu) {
	// destroy sub menu
	submenu_list_t *cur = *submenu;
	
	while (cur) {
		submenu_list_t *td = cur;
		cur = cur->next;
		
		submenuDestroyItem(td);
	}
	
	*submenu = NULL;
}

void menuAddHint(menu_item_t *menu, int text_id, int icon_id) {
	// allocate a new hint item
	menu_hint_item_t* hint = malloc(sizeof(menu_hint_item_t));

	hint->text_id = text_id;
	hint->icon_id = icon_id;
	hint->next = NULL;

	if (menu->hints) {
		menu_hint_item_t* top = menu->hints;

		// rewind to end
		for (; top->next; top = top->next);

		top->next = hint;
	} else {
		menu->hints = hint;
	}
}

void menuRemoveHints(menu_item_t *menu) {
	while (menu->hints) {
		menu_hint_item_t* hint = menu->hints;
		menu->hints = hint->next;
		free(hint);
	}
}

void menuInitHints(menu_item_t* menu) {
	menuRemoveHints(menu);

	menuAddHint(menu, _STR_SETTINGS, START_ICON);
	item_list_t *support = menu->userdata;
	if (!support->enabled)
		menuAddHint(menu, _STR_START_DEVICE, CROSS_ICON);
	else {
		menuAddHint(menu, _STR_RUN, CROSS_ICON);
		if (support->itemGetCompatibility)
			menuAddHint(menu, _STR_COMPAT_SETTINGS, TRIANGLE_ICON);
		if (gEnableDandR) {
			if (support->itemRename)
				menuAddHint(menu, _STR_RENAME, CIRCLE_ICON);
			if (support->itemDelete)
				menuAddHint(menu, _STR_DELETE, SQUARE_ICON);
		}
	}
}

char *menuItemGetText(menu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

char *submenuItemGetText(submenu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

static void swap(submenu_list_t* a, submenu_list_t* b) {
	submenu_list_t *pa, *nb;
	pa = a->prev;
	nb = b->next;
	
	a->next = nb;
	b->prev = pa;
	b->next = a;
	a->prev = b;
	
	if (pa)
		pa->next = b;
	
	if (nb)
		nb->prev = a;
}

// Sorts the given submenu by comparing the on-screen titles
void submenuSort(submenu_list_t** submenu) {
	// a simple bubblesort
	// *submenu = mergeSort(*submenu);
	submenu_list_t *head = *submenu;
	int sorted = 0;
	
	if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
		return;
	
	while (!sorted) {
		sorted = 1;
		
		submenu_list_t *tip = head;
		
		while (tip->next) {
			submenu_list_t *nxt = tip->next;
			
			char *txt1 = submenuItemGetText(&tip->item);
			char *txt2 = submenuItemGetText(&nxt->item);
			
			int cmp = stricmp(txt1, txt2);
			
			if (cmp > 0) {
				swap(tip, nxt);
				
				if (tip == head) 
					head = nxt;
				
				sorted = 0;
			} else {
				tip = tip->next;
			}
		}
	}
	
	*submenu = head;
}

static void menuNextH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->next) {
		selected_item = selected_item->next;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
	}
}

static void menuPrevH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->prev) {
		selected_item = selected_item->prev;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
	}
}

static void menuNextV() {
	if (!selected_item)
		return;
	
	submenu_list_t *cur = selected_item->item->current;
	
	if(cur && cur->next)
		selected_item->item->current = cur->next;
}

static void menuPrevV() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->current;

	// if the current item is on the page start, move the page start one page up before moving the item
	if (selected_item->item->pagestart) {
		if (selected_item->item->pagestart == cur) {
			int itms = ((items_list_t*) gTheme->itemsList->extended)->displayedItems + 1; // +1 because the selection will move as well

			while (--itms && selected_item->item->pagestart->prev)
				selected_item->item->pagestart = selected_item->item->pagestart->prev;
		}
	} else
		selected_item->item->pagestart = cur;

	if(cur && cur->prev) {
		selected_item->item->current = cur->prev;
	}
}

static void menuNextPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->pagestart;

	if (cur) {
		int itms = ((items_list_t*) gTheme->itemsList->extended)->displayedItems + 1;
		while (--itms && cur->next)
			cur = cur->next;

		selected_item->item->current = cur;
	}
}

static void menuPrevPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->pagestart;

	if (cur) {
		int itms = ((items_list_t*) gTheme->itemsList->extended)->displayedItems + 1;
		while (--itms && cur->prev)
			cur = cur->prev;

		selected_item->item->current = cur;
		selected_item->item->pagestart = cur;
	}
}

static void menuFirstPage() {
	if (!selected_item)
		return;

	selected_item->item->current = selected_item->item->submenu;
}

static void menuLastPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->current;

	if (cur) {
		while (cur->next)
			cur = cur->next; // go to end

		selected_item->item->current = cur;

		int itms = ((items_list_t*) gTheme->itemsList->extended)->displayedItems;
		while (--itms && cur->prev) // and move back to have a full page
			cur = cur->prev;

		selected_item->item->pagestart = cur;
	}
}

void menuSetSelectedItem(menu_item_t* item) {
	menu_list_t* itm = menu;
	
	while (itm) {
		if (itm->item == item) {
			selected_item = itm;
			return;
		}
			
		itm = itm->next;
	}
}

void menuRefreshCache(menu_item_t *menu) {
	submenu_list_t *cur = menu->submenu;

	while (cur) {
		if (cur->item.cache_id)
			free(cur->item.cache_id);
		if(cur->item.cache_uid)
			free(cur->item.cache_uid);

		int size = gTheme->gameCacheCount * sizeof(int);
		cur->item.cache_id = malloc(size);
		memset(cur->item.cache_id, -1, size);
		cur->item.cache_uid = malloc(size);
		memset(cur->item.cache_uid, -1, size);

		cur = cur->next;
	}
}

void menuRenderMenu() {
	guiDrawBGPlasma();

	if (!mainMenu)
		return;

	// draw the animated menu
	if (!mainMenuCurrent)
		mainMenuCurrent = mainMenu;

	submenu_list_t* it = mainMenu;

	// calculate the number of items
	int count = 0; int sitem = 0;
	for (; it; count++, it=it->next) {
		if (it == mainMenuCurrent)
			sitem = count;
	}

	int spacing = 25;
	int y = (gTheme->usedHeight >> 1) - (spacing * (count >> 1));
	int cp = 0; // current position
	for (it = mainMenu; it; it = it->next, cp++) {
		// render, advance
		// TODO: Theme support for main menu (font)
		fntRenderString(FNT_DEFAULT, 320, y, ALIGN_CENTER, submenuItemGetText(&it->item), (cp == sitem) ? gTheme->selTextColor : gTheme->textColor);

		y += spacing;
	}
}

void menuHandleInputMenu() {
	if (!mainMenu)
		return;

	if (!mainMenuCurrent)
		mainMenuCurrent = mainMenu;

	if (getKey(KEY_UP)) {
		if (mainMenuCurrent->prev)
			mainMenuCurrent = mainMenuCurrent->prev;
		else	// rewind to the last item
			while (mainMenuCurrent->next)
				mainMenuCurrent = mainMenuCurrent->next;
	}

	if (getKey(KEY_DOWN)) {
		if (mainMenuCurrent->next)
			mainMenuCurrent = mainMenuCurrent->next;
		else
			mainMenuCurrent = mainMenu;
	}

	if (getKeyOn(KEY_CROSS)) {
		// execute the item via looking at the id of it
		int id = mainMenuCurrent->item.id;

		if (id == MENU_SETTINGS) {
			guiShowConfig();
		} else if (id == MENU_GFX_SETTINGS) {
			guiShowUIConfig();
		} else if (id == MENU_IP_CONFIG) {
			guiShowIPConfig();
		} else if (id == MENU_SAVE_CHANGES) {
			saveConfig(CONFIG_OPL | CONFIG_VMODE, 1);
		} else if (id == MENU_START_HDL) {
			handleHdlSrv();
		} else if (id == MENU_ABOUT) {
			guiShowAbout();
		} else if (id == MENU_EXIT) {
			sysExecExit();
		} else if (id == MENU_POWER_OFF) {
			sysPowerOff();
		}

		// so the exit press wont propagate twice
		readPads();
	}

	if(getKeyOn(KEY_START) || getKeyOn(KEY_CIRCLE)) {
		if (gAPPStartMode || gETHStartMode || gUSBStartMode || gHDDStartMode)
			guiSwitchScreen(GUI_SCREEN_MAIN);
	}
}

void menuRenderMain() {
	if (!menu)
		return;

	if (!selected_item)
		selected_item = menu;

	if (!selected_item->item->current)
		selected_item->item->current = selected_item->item->submenu;

	submenu_list_t* cur = selected_item->item->current;

	theme_element_t* elem = gTheme->mainElems.first;
	while (elem) {
		if (elem->drawElem)
			elem->drawElem(selected_item, cur, NULL, elem);

		elem = elem->next;
	}
}

void menuHandleInputMain() {
	if (!selected_item)
		return;

	if(getKey(KEY_LEFT)) {
		menuPrevH();
	} else if(getKey(KEY_RIGHT)) {
		menuNextH();
	} else if(getKey(KEY_UP)) {
		menuPrevV();
	} else if(getKey(KEY_DOWN)){
		menuNextV();
	} else if(getKeyOn(KEY_CROSS)) {
		if (gUseInfoScreen && selected_item->item->current && gTheme->infoElems.first) {
			if (selected_item->item->current->item.id != itemIdConfig) {
				itemIdConfig = selected_item->item->current->item.id;

				if (itemConfig)
					configFree(itemConfig);

				item_list_t *support = selected_item->item->userdata;
				itemConfig = support->itemGetConfig(itemIdConfig);
			}

			guiSwitchScreen(GUI_SCREEN_INFO);
		} else
			selected_item->item->execCross(selected_item->item);
	} else if(getKeyOn(KEY_TRIANGLE)) {
		selected_item->item->execTriangle(selected_item->item);
	} else if(getKeyOn(KEY_CIRCLE)) {
		selected_item->item->execCircle(selected_item->item);
	} else if(getKeyOn(KEY_SQUARE)) {
		selected_item->item->execSquare(selected_item->item);
	} else if(getKeyOn(KEY_START)) {
		// reinit main menu - show/hide items valid in the active context
		menuInitMainMenu();
		guiSwitchScreen(GUI_SCREEN_MENU);
	} else if(getKeyOn(KEY_SELECT)) {
		selected_item->item->refresh(selected_item->item);
	} else if(getKey(KEY_L1)) {
		menuPrevPage();
	} else if(getKey(KEY_R1)) {
		menuNextPage();
	} else if (getKeyOn(KEY_L2)) { // home
		menuFirstPage();
	} else if (getKeyOn(KEY_R2)) { // end
		menuLastPage();
	}
}

void menuRenderInfo() {
	submenu_list_t* cur = selected_item->item->current;

	theme_element_t* elem = gTheme->infoElems.first;
	while (elem) {
		if (elem->drawElem)
			elem->drawElem(selected_item, cur, itemConfig, elem);

		elem = elem->next;
	}
}

void menuHandleInputInfo() {
	if(getKeyOn(KEY_CROSS)) {
		selected_item->item->execCross(selected_item->item);
	} else if(getKeyOn(KEY_CIRCLE)) {
		guiSwitchScreen(GUI_SCREEN_MAIN);
	}
}
