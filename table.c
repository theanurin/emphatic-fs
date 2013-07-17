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

#include <unistd.h>

#include "mfatic-config.h"
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
PRIVATE bool lookup_item (cache_entry_t **found, unsigned int key);

// function to fetch a given FAT sector from the cache.
PRIVATE fat_entry_t * get_sector (unsigned int index);


// global pointer to the volume information for the file system that we
// have mounted.
PRIVATE const fat_volume_t *volume_info;

// global cache structure.
PRIVATE struct fat_cluster_cache cache;


/**
 *  Initialise the pointer to volume information. Should only be called
 *  once at mount time.
 */
    PUBLIC void
table_init (v)
    const fat_volume_t *v;      // pointer to volume information.
{
    volume_info = v;

    // initialise the cache structure's fields.
    cache.lru = cache.mru = NULL;
    cache.available = CACHE_SECTORS_MAX;
}

/**
 *  Read the contents of a given cell in the FAT. Return value is the cell
 *  contents.
 */
    PUBLIC fat_entry_t
get_fat_entry (entry)
    fat_entry_t entry;      // index of the cell to read.
{
    fat_entry_t *sector;
    unsigned int fat_offset, sector_index;

    // get the offset of the required FAT entry in bytes.
    fat_offset = entry * FAT_ENTSIZE;

    // get the index of the sector that contains that entry.
    sector_index = fat_offset / SECTOR_SIZE (volume_info);

    // fetch the sector.
    sector = get_sector (sector_index);

    // calculate the offset of the entry into the sector.
    fat_offset = (fat_offset % SECTOR_SIZE (volume_info)) / 
        sizeof (fat_entry_t);

    // return the FAT entry from within the sector.
    return sector [fat_offset];
}

/**
 *  Write a new value to a particular entry in the FAT. This procedure
 *  uses "no write allocate", ie. if the FAT sector being written to is
 *  not present in the cache, it will not be brought in.
 */
    PUBLIC void
put_fat_entry (entry, val)
    fat_entry_t entry;          // index of FAT entry to write to.
    fat_entry_t val;            // value to write there.
{
    unsigned int offset, index;
    size_t sector_size = SECTOR_SIZE (volume_info);
    fat_entry_t old_val;

    // calculate sector index, and offset within that sector.
    offset = entry * FAT_ENTSIZE;
    index = offset / sector_size;
    offset %= sector_size;

    // seek the device file to the correct byte offset of the FAT entry
    // that is to be written to.
    safe_seek (volume_info->dev_fd, 
      (FAT_START (volume_info) + index) * sector_size + offset,
      SEEK_SET);

    // FAT32 entries are only 28 bits long, and the most significant 4
    // bits are reserved, and must not be overwritten on writes. Instead,
    // we have to read the existing contents, and OR them into the new
    // value.
    safe_read (volume_info->dev_fd, &old_val, sizeof (fat_entry_t));
    safe_seek (volume_info->dev_fd, -1 * sizeof (fat_entry_t), SEEK_CUR);
    val = (old_val & 0xF0000000) | (val & 0x0FFFFFFF);

    // write in the new value.
    safe_write (volume_info->dev_fd, &val, sizeof (fat_entry_t));
}

/**
 *  Add a new cache item to the head of the linked list of cache entries.
 */
    PRIVATE void
add_to_mru (new_item)
    cache_entry_t *new_item;    // item to be added.
{
    cache_entry_t *temp;

    // check the available slot count. If it is zero, we need to evict
    // the least recently used item.
    if (cache.available == 0)
    {
        // unlink and free the least recently used item from the head of
        // the list.
        temp = unlink_item (&(cache.lru));

        // free the sector contents, then the list structure.
        safe_free ((void **) &(temp->sector));
        safe_free ((void **) &temp);
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

    // unlink the item. Remember to increment the number of available
    // cache slots, because one slot becomes free as a result of unlinking.
    *item_pointer = (*item_pointer)->next;
    cache.available += 1;

    return temp;
}

/**
 *  Search for a cache item that matches a given key. If found, that item
 *  will be unlinked from the cache item list, and added to the MRU end,
 *  and the variable pointed to by the found parameter will be set to
 *  point to the cache item. 
 *
 *  Return value is true if an item was found, or false if not.
 */
    PRIVATE bool
lookup_item (found, key)
    cache_entry_t **found;      // address of var to point to found item.
    unsigned int key;           // key to match to.
{
    cache_entry_t **cp;

    // step through the cache from the start of the list (LRU end) until
    // we either find a matching item, or reach the end of the list.
    for (cp = &(cache.lru); (*cp != NULL) && ((*cp)->key != key);
      cp = &((*cp)->next))
    {
        ;
    }

    // have we found a match for the key?
    if (*cp != NULL)
    {
        // yes. Point *found to the matching item, move the item to the
        // MRU end of the list and return true.
        *found = *cp;
        add_to_mru (unlink_item (cp));
        return true;
    }

    // if we reach this point, we did not find a match. Return false.
    return false;
}

/**
 *  Fetch a given sector from within the FAT.
 *
 *  Return value is a pointer to a buffer of size one sector containing
 *  the contents of the FAT sector.
 */
    PRIVATE fat_entry_t *
get_sector (index)
    unsigned int index;         // sector index, from start of FAT.
{
    cache_entry_t *cache_item;

    // first try looking up the item in the cache. Is it already there?
    if (lookup_item (&cache_item, index) == false)
    {
        // not found, so we will need to read the FAT sector in and add it
        // to the cache.
        cache_item = safe_malloc (sizeof (cache_entry_t));
        cache_item->key = index;
        cache_item->sector = safe_malloc (SECTOR_SIZE (volume_info));

        // seek to and read in the FAT sector.
        safe_seek (volume_info->dev_fd,
          (FAT_START (volume_info) + index) * SECTOR_SIZE (volume_info),
          SEEK_SET);
        safe_read (volume_info->dev_fd, cache_item->sector, 
          SECTOR_SIZE (volume_info));

        // add the new cache item to the MRU end of the cache list.
        add_to_mru (cache_item);
    }

    return cache_item->sector;
}


// vim: ts=4 sw=4 et
