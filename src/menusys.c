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
static struct menu_list_t *menu;
static struct menu_list_t *selected_item;

static icon_cache_t cov_cache;
static icon_cache_t ico_cache;
static icon_cache_t bg_cache;

static unsigned int inactiveFrames;

// height of the hint bar at the bottom of the screen
#define HINT_HEIGHT 16

#define PIXMAP_STATE_NOT_FOUND 0
#define PIXMAP_STATE_CACHED 1
#define PIXMAP_STATE_NOT_CACHED 2

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
static void onCacheItemRemoval(void *ccache, cache_entry_t *entry) {
	icon_cache_t *cache = ccache;
	
	if (!cache || !entry || !entry->item)
		return;
		
	// remove the cache entry reference
	entry->item->cache_entry_ref[cache->userid] = NULL;
	
	// the prev. state was not "not found"
	if (entry->item->pixmap_state[cache->userid] != PIXMAP_STATE_NOT_FOUND)
		// move to - "to load" again
		entry->item->pixmap_state[cache->userid] = PIXMAP_STATE_NOT_CACHED;
}

static void menuInitArtCache() {
	cacheInitCache(&ico_cache, "ICO", gCountIconsCache);
	cacheInitCache(&cov_cache, "COV", gCountCoversCache);
	cacheInitCache(&bg_cache, "BG", gCountBackgroundsCache);
	
	ico_cache.userid = 0;
	cov_cache.userid = 1;
	bg_cache.userid = 2;

	ico_cache.cacheItemRemoved = &onCacheItemRemoval;
	cov_cache.cacheItemRemoved = &onCacheItemRemoval;
	bg_cache.cacheItemRemoved = &onCacheItemRemoval;
}

void menuInit() {
	menu = NULL;
	selected_item = NULL;
	inactiveFrames = 0;
	
	menuInitArtCache();
}

void menuEnd() {
	cacheDestroyCache(&ico_cache);
	cacheDestroyCache(&cov_cache);
	cacheDestroyCache(&bg_cache);
	
	// destroy menu
	struct menu_list_t *cur = menu;
	
	while (cur) {
		struct menu_list_t *td = cur;
		cur = cur->next;
		
		if (&td->item)
			submenuDestroy(&td->item->submenu);
		
		menuRemoveHints(td->item);
		
		free(td);
	}
}

static struct menu_list_t* AllocMenuItem(struct menu_item_t* item) {
	struct menu_list_t* it;
	
	it = malloc(sizeof(struct menu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item = item;
	
	return it;
}

void menuAppendItem(struct menu_item_t* item) {
	assert(item);
	
	if (menu == NULL) {
		menu = AllocMenuItem(item);
		return;
	}
	
	struct menu_list_t *cur = menu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct menu_list_t *newitem = AllocMenuItem(item);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
}


static struct submenu_list_t* AllocSubMenuItem(int icon_id, char *text, int id, int text_id) {
	struct submenu_list_t* it;
	
	it = malloc(sizeof(struct submenu_list_t));
	
	it->prev = NULL;
	it->next = NULL;
	it->item.icon_id = icon_id;
	it->item.text = text;
	it->item.text_id = text_id;
	it->item.id = id;
	
	// not sure if we have icon or not yet
	it->item.pixmap_state[0] = PIXMAP_STATE_NOT_CACHED;
	it->item.pixmap_state[1] = PIXMAP_STATE_NOT_CACHED;
	it->item.pixmap_state[2] = PIXMAP_STATE_NOT_CACHED;
	
	// and no cache entry yet
	it->item.cache_entry_ref[0] = NULL;
	it->item.cache_entry_ref[1] = NULL;
	it->item.cache_entry_ref[2] = NULL;
	
	return it;
}

struct submenu_list_t* submenuAppendItem(struct submenu_list_t** submenu, int icon_id, char *text, int id, int text_id) {
	if (*submenu == NULL) {
		*submenu = AllocSubMenuItem(icon_id, text, id, text_id);
		return *submenu; 
	}
	
	struct submenu_list_t *cur = *submenu;
	
	// traverse till the end
	while (cur->next)
		cur = cur->next;
	
	// create new item
	struct submenu_list_t *newitem = AllocSubMenuItem(icon_id, text, id, text_id);
	
	// link
	cur->next = newitem;
	newitem->prev = cur;
	
	return newitem;
}

static void submenuDestroyItem(struct submenu_list_t* cur) {
	cacheRemoveItem(&cov_cache, &cur->item);
	cacheRemoveItem(&ico_cache, &cur->item);
	cacheRemoveItem(&bg_cache, &cur->item);
	
	free(cur);
}

void submenuRemoveItem(struct submenu_list_t** submenu, int id) {
	struct submenu_list_t* cur = *submenu;
	struct submenu_list_t* prev = NULL;	
	
	while (cur) {
		if (cur->item.id == id) {
			struct submenu_list_t* next = cur->next;
			
			if (prev)
				prev->next = cur->next;
			
			if (*submenu == cur)
				*submenu = next;
			
			// inform cache the item vanished, delete
			submenuDestroyItem(cur);
			
			cur = next;
		} else {
			prev = cur;
			cur = cur->next;
		}
	}
}

void submenuDestroy(struct submenu_list_t** submenu) {
	// destroy sub menu
	struct submenu_list_t *cur = *submenu;
	
	while (cur) {
		struct submenu_list_t *td = cur;
		cur = cur->next;
		
		submenuDestroyItem(td);
	}
	
	*submenu = NULL;
}

static char *GetMenuItemText(struct menu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

char *submenuItemGetText(struct submenu_item_t* it) {
	if (it->text_id >= 0)
		return _l(it->text_id);
	else
		return it->text;
}

static void swap(struct submenu_list_t* a, struct submenu_list_t* b) {
	struct submenu_list_t *pa, *nb;
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
void submenuSort(struct submenu_list_t** submenu) {
	// a simple bubblesort
	// *submenu = mergeSort(*submenu);
	struct submenu_list_t *head = *submenu;
	int sorted = 0;
	
	if ((submenu == NULL) || (*submenu == NULL) || ((*submenu)->next == NULL))
		return;
	
	while (!sorted) {
		sorted = 1;
		
		struct submenu_list_t *tip = head;
		
		while (tip->next) {
			struct submenu_list_t *nxt = tip->next;
			
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
	
	struct submenu_list_t *cur = selected_item->item->current;
	
	if(cur && cur->next)
		selected_item->item->current = cur->next;
}

void menuPrevV() {
	if (!selected_item)
		return;

	struct submenu_list_t *cur = selected_item->item->current;

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

	struct submenu_list_t *cur = selected_item->item->pagestart;

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

	struct submenu_list_t *cur = selected_item->item->pagestart;

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

	struct submenu_list_t *cur = selected_item->item->current;

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

struct menu_item_t* menuGetCurrent() {
	if (!selected_item)
		return NULL;

	return selected_item->item;
}

void menuItemExecButton(void (*execActionButton)(struct menu_item_t *self, int id)) {
	if (execActionButton) {

		// selected submenu id. default -1 = no selection
		int subid = -1;

		struct submenu_list_t *cur = selected_item->item->current;
		if (cur)
			subid = cur->item.id;

		execActionButton(selected_item->item, subid);
	}
}

void menuSetSelectedItem(struct menu_item_t* item) {
	struct menu_list_t* itm = menu;
	
	while (itm) {
		if (itm->item == item) {
			selected_item = itm;
			return;
		}
			
		itm = itm->next;
	}
}

void submenuPixmapLoaded(icon_cache_t* cache, void *centry, int result) {
	cache_entry_t *entry = (cache_entry_t *)centry;
	
	if (!entry)
		return;
	
	if (!entry->item)
		return;
	
	// we have to check if the cache didn't already reuse the entry
	if (entry->item->cache_entry_ref[cache->userid] != entry)
		return;
	
	// hmm seems okay, let's set it loaded (or not...)
	entry->item->pixmap_state[cache->userid] = result ? PIXMAP_STATE_CACHED : PIXMAP_STATE_NOT_FOUND;
	
	// reuse the cache entry immediately if the pixmap could not be lodaded
	if (!result)
		cacheReleaseItem(cache, entry);
}

static GSTEXTURE* getCachedTexture(icon_cache_t* cache, struct submenu_item_t* item, GSTEXTURE* dflt) {
	if (!gEnableArt)
		return dflt;

	// does the item have the icon at all? Nope?
	if (item->pixmap_state[cache->userid] == PIXMAP_STATE_NOT_FOUND)
		return dflt;
	
	// do we already have a cache entry? If so, return the cached entry
	if (item->pixmap_state[cache->userid] == PIXMAP_STATE_CACHED) {
		// inform we use this entry still - as it will be displayed!
		cacheInformUsage(item->cache_entry_ref[cache->userid]);
		return &item->cache_entry_ref[cache->userid]->texture;
	}
	
	// Try to cache if possible
	// Does the current menu have a support list, and is the item valid ?
	if (selected_item->item->userdata && (item->id != -1)) {
		// retrieve the support
		item_list_t *support = selected_item->item->userdata;
		
		// not cached yet.
		// under the cache pre-delay (to avoid filling cache while moving around)
		if (inactiveFrames < support->delay)
			return dflt;

		// does it provide Art ? no? return default pixmap
		if (!support->itemGetArt) 
			return dflt;
		
		// reference back to the pointer to cache entry
		cache_entry_t **entryref = &item->cache_entry_ref[cache->userid];
		
		// no matter what happens, the item gets a cache entry assigned if cache has any space left
		// obtain or schedule the retrieval of the pixmap
		if (cacheGetEntry(cache, item, entryref, support)) {
			// cache hit!
			if (*entryref && (*entryref)->texture.Mem)  // but did it produce a valid entry?
				return &(*entryref)->texture;
		}
		
		// if we're here, the texture was not loaded yet or the cache is still full
	}
	
	return dflt;
}

GSTEXTURE* menuGetCurrentArt() {
	if (!selected_item)
		selected_item = menu;

	struct submenu_list_t *cur = selected_item->item->current;
	if (cur)
		return getCachedTexture(&bg_cache, &cur->item, NULL);
	return NULL;
}

void menuDrawStatic() {
	int icnt = gTheme->displayedItems;
	int found = 0;

	if (!menu)
		return;
	
	if (!selected_item) 
		selected_item = menu;

	if (!selected_item->item->current)
		selected_item->item->current = selected_item->item->submenu;

	struct submenu_list_t *cur = selected_item->item->current;
	struct submenu_list_t *ps  = selected_item->item->pagestart;

	// verify the item is in visible range
	while (icnt-- && ps) {
		if (ps == cur) {
			found = 1;
			break;
		}
		ps = ps->next;
	}

	// page not properly aligned?
	if (!found)
		selected_item->item->pagestart = cur;

	// reset to page start after cur. item visibility determination
	ps  = selected_item->item->pagestart;

	//// rendering ELEMENT "Menu Icon"
	GSTEXTURE* someTex = NULL, *otherTex = NULL;
	if (gTheme->menuIcon.enabled) {
		someTex = thmGetTexture(selected_item->item->icon_id);
		if (someTex && someTex->Mem)
			rmDrawPixmap(someTex, gTheme->menuIcon.posX, gTheme->menuIcon.posY, gTheme->menuIcon.aligned, gTheme->menuIcon.width, gTheme->menuIcon.height, gTheme->menuIcon.color);
	}

	//// rendering ELEMENT "Menu Text"
	if (gTheme->menuText.enabled) {
		if (selected_item->prev)
			someTex = thmGetTexture(LEFT_ICON);
		else
			someTex = NULL;
		if (selected_item->next)
			otherTex = thmGetTexture(RIGHT_ICON);

		if (gTheme->menuText.aligned) {
			int offset = gTheme->menuText.width >> 1;
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, gTheme->menuText.posX - offset, gTheme->menuText.posY, gTheme->menuText.aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
			if (otherTex && otherTex->Mem)
				rmDrawPixmap(otherTex, gTheme->menuText.posX + offset, gTheme->menuText.posY, gTheme->menuText.aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		}
		else {
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, gTheme->menuText.posX - someTex->Width, gTheme->menuText.posY, gTheme->menuText.aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
			if (otherTex && otherTex->Mem)
				rmDrawPixmap(otherTex, gTheme->menuText.posX + gTheme->menuText.width, gTheme->menuText.posY, gTheme->menuText.aligned, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
		}
                fntRenderString(FNT_DEFAULT, gTheme->menuText.posX, gTheme->menuText.posY, gTheme->menuText.aligned, GetMenuItemText(selected_item->item), gTheme->menuText.color);
	}
	
	if (cur) {
		//// rendering ELEMENT "Item List"
		theme_element_t* itemsList = &gTheme->itemsList;

		int stretchedSize = 0;
		if (gTheme->itemsListIcons) {
			stretchedSize = 20;
			someTex = thmGetTexture(DISC_ICON);
		}
	
		int curpos = gTheme->itemsList.posY;
		int others = 0;
		u64 color;
		while (ps && (others < gTheme->displayedItems)) {
			if (gTheme->itemsListIcons) {
				otherTex = getCachedTexture(&ico_cache, &ps->item, someTex);
				if (otherTex && otherTex->Mem)
					rmDrawPixmap(otherTex, itemsList->posX, curpos + others * MENU_ITEM_HEIGHT, ALIGN_NONE, stretchedSize, stretchedSize, gDefaultCol);
			}
		
			if (ps == cur)
				color = gTheme->selTextColor;
			else
				color = gTheme->itemsList.color;

                        fntRenderString(FNT_DEFAULT, itemsList->posX + stretchedSize, curpos + others * MENU_ITEM_HEIGHT, ALIGN_NONE, submenuItemGetText(&ps->item), color);
		
			ps = ps->next;
			others++;
		}

		//// rendering ELEMENT "Item Cover"
		if (gTheme->itemCover.enabled) {
			otherTex = thmGetTexture(COVER_OVERLAY);
			if (otherTex && otherTex->Mem) {
				someTex = getCachedTexture(&cov_cache, &cur->item, NULL);
				if (someTex && someTex->Mem) {
					rmDrawOverlayPixmap(otherTex, gTheme->itemCover.posX, gTheme->itemCover.posY, gTheme->itemCover.aligned, gTheme->itemCover.width, gTheme->itemCover.height, gTheme->itemCover.color,
							someTex, gTheme->coverBlend_ulx, gTheme->coverBlend_uly, gTheme->coverBlend_urx, gTheme->coverBlend_ury,
							gTheme->coverBlend_blx, gTheme->coverBlend_bly, gTheme->coverBlend_brx, gTheme->coverBlend_bry);
				}
			}
			else {
				someTex = getCachedTexture(&cov_cache, &cur->item, NULL);
				if (someTex && someTex->Mem)
					rmDrawPixmap(someTex, gTheme->itemCover.posX, gTheme->itemCover.posY, gTheme->itemCover.aligned, gTheme->itemCover.width, gTheme->itemCover.height, gTheme->itemCover.color);
			}
		}

		//// rendering ELEMENT "Item Icon"
		if (gTheme->itemIcon.enabled) {
			someTex = getCachedTexture(&ico_cache, &cur->item, NULL);
			if (someTex && someTex->Mem)
				rmDrawPixmap(someTex, gTheme->itemIcon.posX, gTheme->itemIcon.posY, gTheme->itemIcon.aligned, gTheme->itemIcon.width, gTheme->itemIcon.height, gTheme->itemIcon.color);
		}

		//// rendering ELEMENT "Item Text"
		if (gTheme->itemText.enabled) {
			if (selected_item->item->userdata && (cur->item.id != -1)) {
				item_list_t *support = selected_item->item->userdata;
                                fntRenderString(FNT_DEFAULT, gTheme->itemText.posX, gTheme->itemText.posY, gTheme->itemText.aligned, support->itemGetStartup(cur->item.id), gTheme->itemText.color);
			}
		}
	}

	//// rendering ELEMENT "Hint Text"
	// single hint
	struct menu_hint_item_t* hint = selected_item->item->hints;
	
	if (gTheme->hintText.enabled && hint) {
		int x = gTheme->hintText.posX;
		int y = gTheme->hintText.posY;
		
		// background
		//rmDrawRect(x, y, ALIGN_NONE, gTheme->hintText.width, gTheme->hintText.height, gColDarker);
	
		for (; hint; hint = hint->next) {
			someTex = thmGetTexture(hint->icon_id);
			if (someTex && someTex->Mem) {
				rmDrawPixmap(someTex, x, y, ALIGN_NONE, DIM_UNDEF, DIM_UNDEF, gDefaultCol);
				x += someTex->Width + 2;
			}
			
                        x += fntRenderString(FNT_DEFAULT, x, y, ALIGN_NONE, _l(hint->text_id), gTheme->hintText.color);
			x+= 12;
		}
	}
}

void menuSetInactiveFrames(unsigned int frames) {
	inactiveFrames = frames;
}

void menuAddHint(struct menu_item_t *menu, int text_id, int icon_id) {
	// allocate a new hint item
	struct menu_hint_item_t* hint = malloc(sizeof(struct menu_hint_item_t));
	
	hint->text_id = text_id;
	hint->icon_id = icon_id;
	hint->next = NULL;
	
	if (menu->hints) {
		struct menu_hint_item_t* top = menu->hints;
		
		// rewind to end
		for (; top->next; top = top->next);
		
		top->next = hint;
	} else {
		menu->hints = hint;
	}
}

void menuRemoveHints(struct menu_item_t *menu) {
	while (menu->hints) {
		struct menu_hint_item_t* hint = menu->hints;
		menu->hints = hint->next;
		free(hint);
	}
}

void menuInitHints(struct menu_item_t* menu) {
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
