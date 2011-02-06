#include "include/texcache.h"
#include "include/ioman.h"
#include "include/gui.h"
#include "include/util.h"

static int frameID = 0;
static int inactiveFrames = 0;

void cacheNextFrame(int inactives) {
	++frameID;
	inactiveFrames = inactives;
}

// io call to handle the loading of covers
#define IO_MENU_LOAD_ART 5

typedef struct {
	image_cache_t* cache;
	cache_entry_t* entry;
	item_list_t* list;
	// only for comparison if the deferred action is still valid
	int cacheUID;
	// id of the item in the list
	int itemId;
} load_image_request_t;

// Io handled action...
static void cacheLoadImage(void* data) {
	load_image_request_t* req = data;

	// Safeguards...
	if (!req || !req->entry || !req->cache)
		return;

	item_list_t* handler = req->list;
	if (!handler)
		return;

	// the cache entry was already reused!
	if (/*(!req->entry->qr) || */(req->cacheUID != req->entry->UID)) {
		LOG("cacheLoadImage(%d) - entry was re-used !! requested UID: %d entry UID: %d\n", req->cache->userId, req->cacheUID, req->entry->UID);
		return;
	}

	// seems okay. we can proceed
	GSTEXTURE* texture = &req->entry->texture;
	if(texture->Mem != NULL) {
		free(texture->Mem);
		texture->Mem = NULL;
	}

	LOG("cacheLoadImage(%d) - loading image for UID: %d\n", req->cache->userId, req->cacheUID);

	if (handler->itemGetArt(handler->itemGetStartup(req->itemId), texture, req->cache->prefix, GS_PSM_CT24) < 0)
		req->entry->lastUsed = 0;
	else
		req->entry->lastUsed = frameID;

	req->entry->qr = NULL;

	free(req);
}

void cacheInit() {
	ioRegisterHandler(IO_MENU_LOAD_ART, &cacheLoadImage);
}

void cacheEnd() {
	// nothing to do... others have to destroy the cache via cacheDestroyCache
}

static void cacheClearItem(cache_entry_t* item, int freeTxt) {
	if (freeTxt && item->texture.Mem)
		free(item->texture.Mem);
	
	memset(item, 0, sizeof(cache_entry_t));
	item->texture.Mem = NULL;
	item->texture.Vram = 0;
	item->qr = NULL;
	item->lastUsed = -1;
	item->UID = -1;
}

void cacheInitCache(image_cache_t* cache, int userId, const char* prefix, int count) {
	cache->userId = userId;
	cache->count = count;
	cache->prefix = prefix;
	cache->nextUID = 0;
	cache->content = (cache_entry_t*) malloc(count * sizeof(cache_entry_t));

	int i;
	for (i = 0; i < count; ++i)
		cacheClearItem(&cache->content[i], 0);
}

void cacheDestroyCache(image_cache_t* cache) {
	int i;
	for (i = 0; i < cache->count; ++i) {
		cacheClearItem(&cache->content[i], 1);
	}
	
	free(cache->content);
}

GSTEXTURE* cacheGetTexture(image_cache_t* cache, item_list_t* list, int* cacheId, int* UID, int itemId) {
	if (*cacheId == -2) {
		//LOG("cacheGetTexture(%d) - texture is null for UID: %d itemId: %d\n", cache->userId, *UID, itemId);
		return NULL;
	} else if (*cacheId != -1) {
		cache_entry_t* entry = &cache->content[*cacheId];
		if (entry->UID == *UID) {
			if (entry->qr)
				return NULL;
			else if (entry->lastUsed == 0) {
				LOG("cacheGetTexture(%d) - callback from cacheLoadImage, texture is null for UID: %d itemId: %d\n", cache->userId, *UID, itemId);
				*cacheId = -2;
				return NULL;
			} else {
				entry->lastUsed = frameID;
				return &entry->texture;
			}
		}
		LOG("cacheGetTexture(%d) - outdated UID: %d was at slot: %d entryUID: %d itemId: %d\n", cache->userId, *UID, *cacheId, entry->UID, itemId);

		*cacheId = -1;
	}

	// under the cache pre-delay (to avoid filling cache while moving around)
	if (inactiveFrames < list->delay)
		return NULL;

	cache_entry_t *currEntry, *oldestEntry = NULL;
	int i, rtime = frameID;

	for (i = 0; i < cache->count; i++) {
		currEntry = &cache->content[i];
		if ((!currEntry->qr) && (currEntry->lastUsed < rtime)) {
			oldestEntry = currEntry;
			rtime = currEntry->lastUsed;
			*cacheId = i;
		}
	}

	if (oldestEntry) {
		load_image_request_t* req = malloc(sizeof(load_image_request_t));
		req->cache = cache;
		req->entry = oldestEntry;
		req->list = list;
		req->itemId = itemId;
		req->cacheUID = cache->nextUID;

		cacheClearItem(oldestEntry, 1);
		oldestEntry->qr = req;
		oldestEntry->UID = cache->nextUID;

		*UID = cache->nextUID++;

		LOG("cacheGetTexture(%d) - new request for UID: %d at slot: %d itemId: %d\n", req->cache->userId, *UID, *cacheId, itemId);

		ioPutRequest(IO_MENU_LOAD_ART, req);
	}

	return NULL;
}
