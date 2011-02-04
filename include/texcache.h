#ifndef __TEX_CACHE_H
#define __TEX_CACHE_H

#include <gsToolkit.h>
#include "include/iosupport.h"

/// A single cache entry...
typedef struct {
	GSTEXTURE texture;
	
	// NULL not queued, otherwise queue request record
	void* qr;
	
	// frame counter the icon was used the last time - oldest get rewritten first in case new icon is requested and cache is full. negative numbers mean
	// slot is free and can be used right now
	int lastUsed; 
	
	/// the item which uses this entry in cache
	struct submenu_item_t* item;
} cache_entry_t;


/// One texture cache instance
typedef struct {
	/// User specified ID, not used in any way by the cache code (not even initialized!)
	int userid;

	/// count of entries (copy of the requested cache size upon cache initialization)
	unsigned int count;
	
	/// directory prefix for this cache (if any)
	const char *prefix;
	
	/// function called when item is being removed from cache
	void (*cacheItemRemoved)(void *cache, cache_entry_t *entry);
	
	/// the cache entries itself
	cache_entry_t *cache;
} icon_cache_t;
	
/** Initializes the cache subsystem.
*/
void cacheInit();

/** Terminates the cache. Does nothing currently. Users of this code have to destroy caches via cacheDestroyCache
*/
void cacheEnd(void);

/** sets a new frame id (call once every frame!)
*/
void cacheNextFrame(void);

/** releases a cache entry - use to release the entry without timeouting */
void cacheReleaseItem(icon_cache_t* cache, cache_entry_t* item);

/** Initializes a single cache 
* @param cache the cache to initialize
* @param width the width of the pixmaps
* @param height the height of the pixmaps
* @param prefix a string prefix that gets prepended to the path when loading the pixmap
* @param count the count of items to cache (negative value = use default)
*/
void cacheInitCache(icon_cache_t* cache, const char* prefix, int count);

/** Destroys a given cache (unallocates all memory stored there, disconnects the pixmaps from the usage points).
* @todo Filter the io queue!
*/
void cacheDestroyCache(icon_cache_t* cache);

/** Call when erasing a menu item */
void cacheRemoveItem(icon_cache_t* cache, struct submenu_item_t* item);

/** Retrieves pixmap or schedules load if not found
*/
int cacheGetEntry(icon_cache_t* cache, struct submenu_item_t* item, cache_entry_t **centry, item_list_t* handler);

/// informs the cache the entry is still in use (so that it does not get reused)
void cacheInformUsage(cache_entry_t *centry);

#endif
