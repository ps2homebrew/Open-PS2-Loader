#ifndef __SUBMENU_H
#define __SUBMENU_H

/// a single submenu item
typedef struct submenu_item
{
    /// Icon used for rendering of this item
    int icon_id;

    /// item description
    char *text;

    /// item description in localized form (used if value is not negative)
    int text_id;

    /// item id (MUST BE VALID, we assert it is != -1 to optimize rendering)
    int id;

    int *cache_id;
    int *cache_uid;
} submenu_item_t;

typedef struct submenu_list
{
    struct submenu_item item;

    struct submenu_list *prev, *next;
} submenu_list_t;

void submenuRebuildCache(submenu_list_t *submenu);
submenu_list_t *submenuAppendItem(submenu_list_t **submenu, int icon_id, char *text, int id, int text_id);
void submenuRemoveItem(submenu_list_t **submenu, int id);
void submenuDestroy(submenu_list_t **submenu);
void submenuSort(submenu_list_t **submenu);

char *submenuItemGetText(submenu_item_t *it);

void submenuDestroyItem(submenu_list_t *submenu);

submenu_list_t *submenuAllocItem(int icon_id, char *text, int id, int text_id);

#endif