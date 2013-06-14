/**
 *  fat_alloc.c
 *
 *  Implementation of procedures to manage the allocation policy of the
 *  Emphatic FAT driver.
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "fat.h"
#include "fat_alloc.h"
#include "utils.h"


// this structure is used to keep a list of what regions of contiguous
// clusters are available.
struct free_region
{
    fat_entry_t         start;
    unsigned            length;
    struct free_region  *next;
};


// local function declarations.
PRIVATE void build_free_list (const fat_entry_t *buffer, size_t length,
  struct free_region **list, bool *prev_alloced);


// global list of free regions.
PRIVATE struct free_region *free_map;


/**
 *  Scan through the file allocation table on the device being mounted,
 *  and build a map of where the free clusters are located. This function
 *  should be called once, at mount time.
 */
    PUBLIC void
init_clusters_map (v)
    fat_volume_t *v;    // volume struct for the filesystem being mounted.
{
    struct free_region *current = safe_malloc (sizeof (struct free_region));
    bool prev_alloced = true;
    size_t nr_entries = SECTOR_SIZE (v) / sizeof (fat_entry_t);

    // FAT is stored in an integer number of sectors, so we will read it in
    // blocks of one sector.
    fat_entry_t *entry_buffer = safe_malloc (SECTOR_SIZE (v));

    // make sure the global var points to the start of the list.
    free_map = current;

    // zero all fields in the first list entry, to handle the boundary
    // condition where the device has no available clusters at all.
    current->length = 0;

    // seek the device to the start of the FAT.
    safe_lseek (v->dev_fd, FAT_START (v) * SECTOR_SIZE (v), SEEK_SET);

    // step through each sector of the FAT, and process all the FAT entries
    // in it.
    for (int i = 0; i < FAT_SECTORS (v); i ++)
    {
        // read the next sector from the FAT.
        safe_read (v->dev_fd, (void *) entry_buffer, SECTOR_SIZE (v));

        // step through the FAT entries, building the free list.
        build_free_list (entry_buffer, nr_entries, &current, 
          &prev_alloced);
    }

    // release the memory allocated to our sector buffer.
    safe_free (&entry_buffer);
}

/**
 *  Step through a buffer of FAT entries, and build a list of free regions.
 */
    PRIVATE void
build_free_list (buffer, length, list, prev_alloced)
    const fat_entry_t *buffer;  // buffer of FAT entries.
    size_t length;              // number of entries in the buffer.
    struct free_region **list;  // points to the pointer to end of list.
    bool *prev_alloced;         // true if the last FAT entry allocated.
{
    struct free_region *new;

    // step through each FAT entry in the buffer, checking if it is marked
    // as free. Any entry which does not contain the free cluster sentinel
    // is deemed to be already allocated.
    for (int i = 0; i < length; i ++)
    {
        // we can use this macro to compare a FAT entry to the free cluster
        // sentinel.
        if (IS_FREE_CLUSTER (buffer [i]))
        {
            // If this is the first cluster in a free region, we will need
            // to create a new entry in the list.
            if (*prev_alloced != false)
            {
                *prev_alloced = false;

                // create the new item, and link into the list.
                new = safe_malloc (sizeof (struct free_region));
                new->start = buffer [i];
                new->length = 1;
                new->next = NULL;
                (*list)->next = new;
                *list = new;
            }
            else
            {
                // not the first free cluster, so we just need to increment
                // the length of the current region.
                (*list)->length += 1;
            }
        }
        else
        {
            // record for the next iteration that the last FAT entry was
            // allocated to a file.
            *prev_alloced = true;
        }
    }
}



// vim: ts=4 sw=4 et
