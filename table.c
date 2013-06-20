/**
 *  table.c
 *
 *  Provides routines to retrieve and modify entries in the file 
 *  allocation table (FAT). Internally, this module caches FAT sectors
 *  on an LRU (least recently used) basis, with write through, meaning
 *  writes to a FAT entry go straight to the hardware, and the cache is
 *  always consistent with the state of the FAT. This should not have
 *  an adverse effect on performance, because writes would normally be
 *  much less frequent than reads, however this hypothesis could be the
 *  subject of testing...
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "utils.h"
#include "fat.h"
#include "table.h"


// Each item in the cache contains a key (the sector index, where 0 is
// the first sector in the FAT) and a pointer to the data from that sector.
typedef struct cache_entry
{
    unsigned int            key;
    fat_entry_t             *sector;
    struct cache_entry      *next;
}
cache_entry_t;

// At the top level, the cache needs two pointers, to the head and tail of
// the linked list of cache items, and also the number of "available 
// slots", or the number of items that are permitted to be added to the
// list. The lru field points to the head of the list, where the least
// recently used item is removed; and mru points to the tail of the list
// where the most recently used item is appended.
struct fat_cluster_cache
{
    cache_entry_t           *lru;
    cache_entry_t           *mru;
    unsigned int            available;
};


// local procedures for manipulating linked lists of cache items.
PRIVATE void add_to_mru (cache_entry_t *new_item);
PRIVATE cache_entry_t * unlink_item (cache_entry_t **item_pointer);
PRIVATE bool lookup_key (cache_entry_t **found, unsigned int key);


// global pointer to the volume information for the file system that we
// have mounted.
PRIVATE fat_volume_t *volume_info;

// global cache structure.
PRIVATE struct fat_cluster_cache cache;


/**
 *  Initialise the pointer to volume information. Should only be called
 *  once at mount time.
 */
    PUBLIC void
table_init (v)
    fat_volume_t *v;    // pointer to volume information.
{
    volum_info = v;

    // initialise the cache structure's fields.
    cache.head = cache.tail = NULL;
    cache.available = CACHE_SECTORS_MAX;
}

/**
 *  Add a new cache item to the head of the linked list of cache entries.
 */
    PRIVATE void
add_to_mru (new_item)
    cache_entry_t *new_item;    // item to be added.
{
    // check the available slot count. If it is zero, we need to evict
    // the least recently used item.
    if (cache.available == 0)
    {
        // unlink and free the least recently used item from the head of
        // the list.
        temp = unlink_item (&(cache.lru));

        // free the sector contents, then the list structure.
        safe_free (&(temp->sector));
        safe_free (&temp);
    }
    else
    {
        cache.available -= 1;
    }

    // link the new item into the list.
    new_item->next = cache.mru;
    cache.mru = new_item;

    if (cache.lru == NULL)
        cache.lru = new_item;
}

/**
 *  unlink an item from a linked list of cache entries, given a pointer
 *  to the next field of the preceding item.
 *
 *  Return value is a pointer to the item that was unlinked from the list.
 */
    PRIVATE cache_entry_t *
unlink_item (item_pointer)
    cache_entry_t **item_pointer;
{
    cache_entry_t *temp = *item_pointer;

    // unlink the item.
    *item_pointer = (*item_pointer)->next;

    return temp;
}


// vim: ts=4 sw=4 et
