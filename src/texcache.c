#include "include/texcache.h"
#include "include/ioman.h"
#include "include/gui.h"
#include "include/menusys.h"
#include "include/util.h"
#include <stdio.h>

static int frameID = 0;

// count of frames till the cache entry is considered to be old and not used any more (~2s)
#define IDLE_FRAME_COUNT (100) 

// io call to handle the loading of covers
#define IO_MENU_LOAD_ART 5
typedef struct {
	icon_cache_t *cache;
	cache_entry_t *entry;
	item_list_t *list;
	// only for comparison if the deferred action is still valid
	struct submenu_item_t* item;
	// id of the icon
	int id;
} load_art_request_t;

// forward decl.
static void cacheLoadArt(void* data);


void cacheInit() {
	ioRegisterHandler(IO_MENU_LOAD_ART, &cacheLoadArt);
}

void cacheEnd(void) {
	// nothing to do...
	// others have to destroy the cache via cacheDestroyCache
}

void cacheNextFrame(void) {
	++frameID;
}

void cacheInitCache(icon_cache_t* cache, const char* prefix, int count) {
	cache->count = count;
	cache->prefix = prefix;
	cache->cacheItemRemoved = NULL;
	cache->cache = (cache_entry_t*)malloc(count * sizeof(cache_entry_t));
	
	int i;
	
	for (i = 0; i < cache->count; ++i) {
		cache_entry_t *entry = &cache->cache[i];
		
		memset(entry, 0, sizeof(cache_entry_t));
		entry->texture.Mem = NULL;
		entry->texture.Vram = 0;
		entry->qr = NULL;
		entry->lastUsed = -1;
		entry->item = NULL;
	}
}

static void cacheClearItem(icon_cache_t* cache, cache_entry_t* item) {
	// if there is a callback on remove, call it now
	if (cache->cacheItemRemoved)
		cache->cacheItemRemoved(cache, item);
	
	if (item->texture.Mem) {
		free(item->texture.Mem);
		item->texture.Mem = NULL;
	}
	
	memset(item, 0, sizeof(cache_entry_t));
	item->texture.Vram = 0;
	item->qr = NULL;
	item->lastUsed = -1;
	item->item = NULL;
}

void cacheReleaseItem(icon_cache_t* cache, cache_entry_t* item) {
	cacheClearItem(cache, item);
}

void cacheDestroyCache(icon_cache_t* cache) {
	int i;
	
	for (i = 0; i < cache->count; ++i) {
		cacheClearItem(cache, &cache->cache[i]);
	}
	
	free(cache->cache);
}

void cacheRemoveItem(icon_cache_t* cache, struct submenu_item_t* item) {
	int i;
	
	for (i = 0; i < cache->count; ++i) {
		cache_entry_t *entry = &cache->cache[i];

		if (entry->item == item) {
			// Is it queued already? If so we have to clear it from queue
			if (entry->qr) {
				// invalidate the request
				((load_art_request_t*)(entry->qr))->list = NULL;
				// TODO: Locks via sema obj - this can cause a race condition!
			}

			cacheClearItem(cache, entry); 
		}
	}
}

/** find index of a candidate for removal (to replace when loading other items).
* @param cache The cache to search
* @param frameid The current frame id
* @return negative number if there is no candidate, index of candidate if such exists
*/ 
static cache_entry_t * cacheFindCandidate(icon_cache_t* cache, struct submenu_item_t* item) {
	int i;
	
	// first loop - scan for free slots only...
	for (i = 0; i < cache->count; ++i) {
		cache_entry_t *entry = &cache->cache[i];
		
		if (entry->item == item) // already cached, just return we found it
			return entry;
		
		if (entry->lastUsed < 0)
			return entry;
	}
	
	cache_entry_t *torecycle = NULL;
	int rtime = frameID;
	
	// second loop - scan for timeouted pixmaps...
	for (i = 0; i < cache->count; ++i) {
		cache_entry_t *entry = &cache->cache[i];
		
		// the entry was not used for a long time enough
		if ((entry->qr == NULL) && ((frameID - entry->lastUsed) > IDLE_FRAME_COUNT)) {
				// older than the prev. candidate?
				if (entry->lastUsed < rtime) {
					torecycle = entry;
					rtime = entry->lastUsed;
				}
		}
	}
	
	if (torecycle) {
		// recycle
		cacheClearItem(cache, torecycle);
		// and return
		return torecycle;
	}
	
	return NULL;
}

int cacheGetEntry(icon_cache_t* cache, struct submenu_item_t* item, cache_entry_t **centry, item_list_t* handler) {
	// valid input?
	if (!centry || !cache || !centry || !handler)
		return 0;
	
	// client already has a cache entry
	if (*centry) {
		// an item is retrieved. synchronise the lastUsed
		// either still queued or texture not zero
		if ((*centry)->qr != NULL || (*centry)->texture.Mem)
			(*centry)->lastUsed = frameID;
		
		return ((*centry)->qr == NULL); // if queue requested, return false
	}
	
	// no cache entry yet on client side, see what the situation is
	cache_entry_t *entry = cacheFindCandidate(cache, item);
		
	if (!entry) // no candidate
		return 0;
	
	// candidate found, link back
	(*centry) = entry;
	
	if (entry->item == item) { // found a match - the item was cached but not linked!
		if (entry->qr != NULL || entry->texture.Mem)
			entry->lastUsed = frameID;
		
		return (entry->qr == NULL); // if queue requested, return false
	}
	
	// found a candidate which was used for other menu item previously
	// reuse the candidate
	entry->lastUsed = frameID;
	entry->item = item;
	entry->qr = NULL;
	
	load_art_request_t* req = malloc(sizeof(load_art_request_t));
	entry->qr = req;
	
	// not linking in the menu item. it can be invalid when the request comes to processing
	// it will be searched for at the time the icon retrieval happens again
	req->cache = cache;
	req->entry = entry;
	req->list = handler;
	req->item = item;
	req->id = item->id;
	
	ioPutRequest(IO_MENU_LOAD_ART, req);

	return 0;
}

// Io handled action...
static void cacheLoadArt(void* data) {
	load_art_request_t* req = data;
	
	// Safeguards...
	if (!req)
		return;
	
	item_list_t* handler = req->list;

	if (!handler || !req->entry || !req->entry->item)
		return;
	
	if (!req->item)
		return;
	
	// the cache entry was already reused!
	if ((!req->entry->qr) || (req->item != req->entry->item))
		return;
	
	// different id for some reason
	if ((req->id != req->item->id))
		return;

	if (!handler->itemGetArt)
		return;
	
	if (!handler->itemGetStartup) {
		LOG("IO: No item get startup %p!\n", handler);
		return;
	}
	
	// seems okay. we can proceed
	GSTEXTURE* texture = &req->entry->texture;
	if(texture->Mem != NULL) {
		free(texture->Mem);
		texture->Mem = NULL;
	}
	int result = handler->itemGetArt(handler->itemGetStartup(req->id), texture, req->cache->prefix, GS_PSM_CT24);
	
	// okay, the pixmap was loaded (or not if result is not null :))
	// inform via deferred update about that
	
	struct gui_update_t *op = guiOpCreate(GUI_OP_PIXMAP_LOADED);
	op->menu.menu = NULL;
	op->menu.subMenu = NULL;
	
	op->pixmap.result = (result >= 0) ? 1 : 0;
	op->pixmap.cache = req->cache;
	op->pixmap.entry = req->entry;
	
	guiDeferUpdate(op);
		
	req->entry->qr = NULL;
	
	// free the request as it is not needed any more
	free(req);
}

void cacheInformUsage(cache_entry_t *centry) {
	centry->lastUsed = frameID;
}
