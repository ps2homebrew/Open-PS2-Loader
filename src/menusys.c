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
#include "include/texcache.h"
#include "assert.h"

// global menu variables
static menu_list_t *menu;
static menu_list_t *selected_item;

static image_cache_t cov_cache;
static image_cache_t ico_cache;
static image_cache_t bg_cache;

// height of the hint bar at the bottom of the screen
#define HINT_HEIGHT 16

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit() {
	menu = NULL;
	selected_item = NULL;
	
	cacheInitCache(&ico_cache, 0, "ICO", gCountIconsCache);
	cacheInitCache(&cov_cache, 1, "COV", gCountCoversCache);
	cacheInitCache(&bg_cache, 2, "BG", gCountBackgroundsCache);
}

void menuEnd() {
	cacheDestroyCache(&ico_cache);
	cacheDestroyCache(&cov_cache);
	cacheDestroyCache(&bg_cache);
	
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
	it->item.cache_id[0] = -1;
	it->item.cache_id[1] = -1;
	it->item.cache_id[2] = -1;
	it->item.cache_uid[0] = -1;
	it->item.cache_uid[1] = -1;
	it->item.cache_uid[2] = -1;
	
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
			
			free(cur);
			
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
		
		free(td);
	}
	
	*submenu = NULL;
}

static char *GetMenuItemText(menu_item_t* it) {
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

void menuNextH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->next) {
		selected_item = selected_item->next;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
	}
}

void menuPrevH() {
	if (!selected_item) {
		selected_item = menu;
	}
	
	if(selected_item->prev) {
		selected_item = selected_item->prev;
		
		if (!selected_item->item->current)
			selected_item->item->current = selected_item->item->submenu;
	}
}

void menuNextV() {
	if (!selected_item)
		return;
	
	submenu_list_t *cur = selected_item->item->current;
	
	if(cur && cur->next)
		selected_item->item->current = cur->next;
}

void menuPrevV() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->current;

	// if the current item is on the page start, move the page start one page up before moving the item
	if (selected_item->item->pagestart) {
		if (selected_item->item->pagestart == cur) {
			int itms = gTheme->displayedItems + 1; // +1 because the selection will move as well

			while (--itms && selected_item->item->pagestart->prev)
				selected_item->item->pagestart = selected_item->item->pagestart->prev;
		}
	} else
		selected_item->item->pagestart = cur;

	if(cur && cur->prev) {
		selected_item->item->current = cur->prev;
	}
}

void menuNextPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->pagestart;

	if (cur) {
		int itms = gTheme->displayedItems + 1;
		while (--itms && cur->next)
			cur = cur->next;

		selected_item->item->current = cur;
	}
}

void menuPrevPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->pagestart;

	if (cur) {
		int itms = gTheme->displayedItems + 1;
		while (--itms && cur->prev)
			cur = cur->prev;

		selected_item->item->current = cur;
		selected_item->item->pagestart = cur;
	}
}

void menuFirstPage() {
	if (!selected_item)
		return;

	selected_item->item->current = selected_item->item->submenu;
}

void menuLastPage() {
	if (!selected_item)
		return;

	submenu_list_t *cur = selected_item->item->current;

	if (cur) {
		while (cur->next)
			cur = cur->next; // go to end

		selected_item->item->current = cur;

		int itms = gTheme->displayedItems;
		while (--itms && cur->prev) // and move back to have a full page
			cur = cur->prev;

		selected_item->item->pagestart = cur;
	}
}

menu_item_t* menuGetCurrent() {
	if (!selected_item)
		return NULL;

	return selected_item->item;
}

void menuItemExecButton(void (*execActionButton)(menu_item_t *self, int id)) {
	if (execActionButton) {

		// selected submenu id. default -1 = no selection
		int subid = -1;

		submenu_list_t *cur = selected_item->item->current;
		if (cur)
			subid = cur->item.id;

		execActionButton(selected_item->item, subid);
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

static GSTEXTURE* getCachedTexture(image_cache_t* cache, submenu_item_t* item, GSTEXTURE* dflt) {
	if (!gEnableArt)
		return dflt;

	// Try to cache if possible
	// Does the current menu have a support list, and is the item valid ?
	if (selected_item->item->userdata && (item->id != -1)) {
		// retrieve the support
		item_list_t *support = selected_item->item->userdata;

		GSTEXTURE* cacheTxt = cacheGetTexture(cache, support, &item->cache_id[cache->userId], &item->cache_uid[cache->userId], item->id);
		if (cacheTxt && cacheTxt->Mem)  // but did it produce a valid entry?
			return cacheTxt;
	}
	
	return dflt;
}

GSTEXTURE* menuGetCurrentArt() {
	if (!selected_item)
		selected_item = menu;

	submenu_list_t *cur = selected_item->item->current;
	if (cur)
		return getCachedTexture(&bg_cache, &cur->item, NULL);
	return NULL;
}

/*void menuDrawImage(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	if (elem->userAttribute) {
		// thmConfig->getAttributeValue(elem->userAttribute);
		//
	} else {

	}

	if (elem->cache) {

	} else {

	}
}*/

void menuDrawMenuIcon(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	GSTEXTURE* someTex = thmGetTexture(curMenu->item->icon_id);
	if (someTex && someTex->Mem)
		rmDrawPixmap(someTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, elem->color);
}

void menuDrawMenuText(menu_list_t *curMenu, submenu_list_t *curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Menu Text"
	GSTEXTURE* someTex = NULL, *otherTex = NULL;
	if (elem->enabled) {
		if (curMenu->prev)
			someTex = thmGetTexture(LEFT_ICON);
		else
			someTex = NULL;
		if (curMenu->next)
			otherTex = thmGetTexture(RIGHT_ICON);

		if (elem->aligned) {
			int offset = elem->width >> 1;
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, elem->posX - offset, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
			if (otherTex && otherTex->Mem)
				rmDrawPixmap(otherTex, elem->posX + offset, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		}
		else {
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, elem->posX - someTex->Width, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
			if (otherTex && otherTex->Mem)
				rmDrawPixmap(otherTex, elem->posX + elem->width, elem->posY, elem->aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		}
		fntRenderString(elem->font, elem->posX, elem->posY, elem->aligned, GetMenuItemText(curMenu->item), elem->color);
	}
}

void menuDrawItemList(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Item List"
	if (curItem) {
		int icnt = gTheme->displayedItems;
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

		GSTEXTURE* someTex = NULL, *otherTex = NULL;
		int stretchedSize = 0;
		if (gTheme->itemsListIcons) {
			stretchedSize = 20;
			someTex = thmGetTexture(DISC_ICON);
		}

		int curpos = elem->posY;
		int others = 0;
		u64 color;
		while (ps && (others < gTheme->displayedItems)) {
			if (gTheme->itemsListIcons) {
				otherTex = getCachedTexture(&ico_cache, &ps->item, someTex);
				if (otherTex && otherTex->Mem)
					rmDrawPixmap(otherTex, elem->posX, curpos + others * MENU_ITEM_HEIGHT, ALIGN_NONE, stretchedSize, stretchedSize, gDefaultCol);
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

void menuDrawItemCover(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Item Cover"
	if (curItem) {
		GSTEXTURE* someTex = NULL, *otherTex = NULL;
		otherTex = thmGetTexture(COVER_OVERLAY);
		if (otherTex && otherTex->Mem) {
			someTex = getCachedTexture(&cov_cache, &curItem->item, NULL);
			if (someTex && someTex->Mem) {
				rmDrawOverlayPixmap(otherTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, elem->color,
						someTex, gTheme->coverBlend_ulx, gTheme->coverBlend_uly, gTheme->coverBlend_urx, gTheme->coverBlend_ury,
						gTheme->coverBlend_blx, gTheme->coverBlend_bly, gTheme->coverBlend_brx, gTheme->coverBlend_bry);
			}
		}
		else {
			someTex = getCachedTexture(&cov_cache, &curItem->item, NULL);
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, elem->color);
		}
	}
}

void menuDrawItemIcon(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Item Icon"
	if (curItem) {
		GSTEXTURE* someTex = getCachedTexture(&ico_cache, &curItem->item, NULL);
		if (someTex && someTex->Mem)
			rmDrawPixmap(someTex, elem->posX, elem->posY, elem->aligned, elem->width, elem->height, elem->color);
	}
}

void menuDrawItemText(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Item Text"
	if (curItem) {
		if (curMenu->item->userdata && (curItem->item.id != -1)) {
			item_list_t *support = curMenu->item->userdata;
			fntRenderString(elem->font, elem->posX, elem->posY, elem->aligned, support->itemGetStartup(curItem->item.id), elem->color);
		}
	}
}

void menuDrawMenuHint(menu_list_t* curMenu, submenu_list_t* curItem, theme_element_t* elem) {
	//// rendering ELEMENT "Hint Text"
	menu_hint_item_t* hint = curMenu->item->hints;
	if (hint) {
		GSTEXTURE* someTex = NULL;
		int x = elem->posX;
		int y = elem->posY;

		for (; hint; hint = hint->next) {
			someTex = thmGetTexture(hint->icon_id);
			if (someTex && someTex->Mem) {
				rmDrawPixmap(someTex, x, y, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
				x += someTex->Width + 2;
			}

			x += fntRenderString(elem->font, x, y, ALIGN_NONE, _l(hint->text_id), elem->color);
			x += 12;
		}
	}
}

void menuDrawStatic() {
	if (!menu)
		return;
	
	if (!selected_item) 
		selected_item = menu;

	if (!selected_item->item->current)
		selected_item->item->current = selected_item->item->submenu;

	submenu_list_t *cur = selected_item->item->current;

	//// rendering ELEMENT "Menu Icon"
	if (gTheme->menuIcon.enabled)
		menuDrawMenuIcon(selected_item, cur, &gTheme->menuIcon);

	//// rendering ELEMENT "Menu Text"
	if (gTheme->menuText.enabled)
		menuDrawMenuText(selected_item, cur, &gTheme->menuText);
	
	//// rendering ELEMENT "Item List"
	if (gTheme->itemsList.enabled)
		menuDrawItemList(selected_item, cur, &gTheme->itemsList);

	//// rendering ELEMENT "Item Cover"
	if (gTheme->itemCover.enabled)
		menuDrawItemCover(selected_item, cur, &gTheme->itemCover);

	//// rendering ELEMENT "Item Icon"
	if (gTheme->itemIcon.enabled)
		menuDrawItemIcon(selected_item, cur, &gTheme->itemIcon);

	//// rendering ELEMENT "Item Text"
	if (gTheme->itemText.enabled)
		menuDrawItemText(selected_item, cur, &gTheme->itemText);

	//// rendering ELEMENT "Hint Text"
	if (gTheme->hintText.enabled)
		menuDrawMenuHint(selected_item, cur, &gTheme->hintText);
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
