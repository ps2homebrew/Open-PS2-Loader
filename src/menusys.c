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
#include "assert.h"

// global menu variables
static menu_list_t *menu;
static menu_list_t *selected_item;

// -------------------------------------------------------------------------------------------
// ---------------------------------------- Menu manipulation --------------------------------
// -------------------------------------------------------------------------------------------
void menuInit() {
	menu = NULL;
	selected_item = NULL;
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
	it->item.config = NULL;
	
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
	if (submenu->item.config)
		configFree(submenu->item.config);

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

void menuNextPage() {
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

void menuPrevPage() {
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

		int itms = ((items_list_t*) gTheme->itemsList->extended)->displayedItems;
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

void menuDrawStatic() {
	if (!menu)
		return;
	
	if (!selected_item) 
		selected_item = menu;

	if (!selected_item->item->current)
		selected_item->item->current = selected_item->item->submenu;

	submenu_list_t* cur = selected_item->item->current;

	theme_element_t* elem = gTheme->elems;
	while (elem) {
		if (elem->drawElem)
			elem->drawElem(selected_item, cur, elem);

		elem = elem->next;
	}
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
