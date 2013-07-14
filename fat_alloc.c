/**
 *  fat_alloc.c
 *
 *  Implementation of procedures to manage the allocation policy of the
 *  Emphatic FAT driver.
 *
 *  Author: Matthew Signorini
 */

#include <unistd.h>     // needed for SEEK_SET

#include "mfatic-config.h"
#include "const.h"
#include "utils.h"
#include "fat.h"
#include "table.h"
#include "fat_alloc.h"


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

// functions for reading salient items from the free space map.
PRIVATE fat_cluster_t get_nearest_free (fat_cluster_t near);
PRIVATE struct free_region * get_largest_region (void);

// higher order function for traversing the free space map to find a
// desired region.
PRIVATE struct free_region ** traverse_map (
  struct free_region ** (*callback) (struct free_region **current,
      struct free_region **candidate, fat_cluster_t cmp), 
  fat_cluster_t cluster);

// callbacks given to traverse_map by get_nearest and get_largest.
PRIVATE struct free_region ** nearest (struct free_region **current,
  struct free_region **candidate, fat_cluster_t cmp);
PRIVATE struct free_region ** largest (struct free_region **current,
  struct free_region **candidate, fat_cluster_t cmp);

PRIVATE unsigned int distance (struct free_region *region, 
  fat_cluster_t cluster);

// merge a newly freed cluster into two neighbouring free regions.
PRIVATE void merge_free_regions (struct free_region **left, 
  struct free_region **right, fat_cluster_t released);
PRIVATE struct free_region * merge_region (struct free_region *reg,
  fat_cluster_t released);


// global list of free regions.
PRIVATE struct free_region *free_map;

// variables used to track statistics.
PRIVATE int nr_allocated_clusters;
PRIVATE int nr_available_clusters;


/**
 *  Scan through the file allocation table on the device being mounted,
 *  and build a map of where the free clusters are located. This function
 *  should be called once, at mount time.
 */
    PUBLIC void
init_clusters_map (v)
    const fat_volume_t *v;  // volume struct for the mounted filesystem.
{
    struct free_region *current = safe_malloc (sizeof (struct free_region));
    bool prev_alloced = true;
    size_t nr_entries = SECTOR_SIZE (v) / sizeof (fat_entry_t);

    nr_allocated_clusters = 0;
    nr_available_clusters = 0;

    // FAT is stored in an integer number of sectors, so we will read it in
    // blocks of one sector.
    fat_entry_t *entry_buffer = safe_malloc (SECTOR_SIZE (v));

    // make sure the global var points to the start of the list.
    free_map = current;

    // zero all fields in the first list entry, to handle the boundary
    // condition where the device has no available clusters at all.
    current->length = 0;

    // seek the device to the start of the FAT.
    safe_seek (v->dev_fd, FAT_START (v) * SECTOR_SIZE (v), SEEK_SET);

    // step through each sector of the FAT, and process all the FAT entries
    // in it.
    for (unsigned int i = 0; i < FAT_SECTORS (v); i ++)
    {
        // read the next sector from the FAT.
        safe_read (v->dev_fd, (void *) entry_buffer, SECTOR_SIZE (v));

        // step through the FAT entries, building the free list.
        build_free_list (entry_buffer, nr_entries, &current, 
          &prev_alloced);
    }

    // release the memory allocated to our sector buffer.
    safe_free ((void **) &entry_buffer);
}

/**
 *  Return the number of clusters which are allocated to files.
 */
    PUBLIC int
used_clusters (void)
{
    return nr_allocated_clusters;
}

/**
 *  Return the number of clusters which are available for allocation.
 */
    PUBLIC int
free_clusters (void)
{
    return nr_available_clusters;
}

/**
 *  Allocate a new cluster to the end of a file. Emphatic's policy is to
 *  allocate the nearest free cluster to the end of the file.
 *
 *  This procedure will mark the chosen cluster as allocated, store the
 *  end of chain sentinel in it's FAT cell, and store the cluster index
 *  of the chosen cluster in the FAT cell of the previous cluster in the
 *  chain. The upshot of all this is that the caller does not have to
 *  do any manipulation of the FAT.
 */
    PUBLIC fat_cluster_t
new_cluster (near)
    fat_cluster_t near;         // current end of chain.
{
    fat_cluster_t chosen = get_nearest_free (near);

    // store the new allocation in the FAT.
    put_fat_entry (chosen, END_CLUSTER_MARK);
    put_fat_entry (near, chosen);

    // update the cluster allocation stats.
    nr_allocated_clusters += 1;
    nr_available_clusters -= 1;

    return chosen;
}

/**
 *  This procedure should be called whenever a cluster is released back
 *  to the pool of free clusters (eg. when a file is permanently deleted).
 *  It will record the cluster as free in the FAT, and will also update
 *  the free space map to reflect the new state.
 */
    PUBLIC void
release_cluster (c)
    fat_cluster_t c;            // cluster offset, from start of data.
{
    struct free_region **left, **right, *temp = NULL;

    // to start off, *left must equal NULL, or merge_free_regions will
    // die.
    left = &temp;

    // The cluster being released must sit somewhere between two free
    // regions. Step through the list of free regions until we find the
    // bordering regions.
    for (right = &free_map; (*right != NULL) && ((*right)->start < c); 
      right = &((*right)->next))
    {
        left = right;
    }

    merge_free_regions (left, right, c);

    // record the cluster as being available.
    put_fat_entry (c, 0x00000000);

    // update the allocation stats.
    nr_allocated_clusters -= 1;
    nr_available_clusters += 1;
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
    for (unsigned int i = 0; i < length; i ++)
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

            // record stats.
            nr_available_clusters += 1;
        }
        else
        {
            // record for the next iteration that the last FAT entry was
            // allocated to a file.
            *prev_alloced = true;

            // record stats.
            nr_allocated_clusters += 1;
        }
    }
}

/**
 *  locate the nearest free cluster to a given cluster, and modify the
 *  free space map to indicate that it is used.
 *
 *  Return value is the cluster which was selected.
 */
    PRIVATE fat_cluster_t
get_nearest_free (near)
    fat_cluster_t near;         // find the closest cluster to this one.
{
    // look up the nearest free region.
    struct free_region **reg = traverse_map (&nearest, near), *temp;
    fat_cluster_t alloc;

    // The cluster to allocate will be on either end of the free region,
    // depending on whether near is on it's left or right.
    if (near < (*reg)->start)
    {
        // allocate the first cluster in the region.
        alloc = (*reg)->start;
        (*reg)->start += 1;
    }
    else
    {
        // allocate the last cluster in the free region.
        alloc = (*reg)->start + (*reg)->length - 1;
    }

    // decrement the length of the free region. If we have just allocated
    // the last cluster, we will have to remove it from the free map.
    (*reg)->length -= 1;

    if ((*reg)->length == 0)
    {
        // unlink reg from the list.
        temp = *reg;
        *reg = (*reg)->next;
        safe_free ((void **) &temp);
    }

    return alloc;
}

/**
 *  Find the largest free region on the volume.
 */
    PRIVATE struct free_region *
get_largest_region (void)
{
    return *(traverse_map (&largest, 0));
}

/**
 *  Traverse the free space map and return a desireable region (ie. the
 *  largest region, or the closest to an existing allocated cluster).
 *
 *  This function takes two parameters, a pointer to a function which will
 *  be invoked on each item in the free space list, and a cluster index,
 *  for finding the closest free region (this may be 0 otherwise).
 */
    PRIVATE struct free_region **
traverse_map (callback, cluster)
    struct free_region ** (*callback) (struct free_region **current, 
      struct free_region **candidate, fat_cluster_t cmp);
    fat_cluster_t cluster;      // cluster to locate closest free region.
{
    struct free_region **current = &free_map, **candidate;

    // step through all the items in the free space map.
    for (candidate = &free_map; *candidate != NULL; 
      candidate = &((*candidate)->next))
    {
        current = callback (current, candidate, cluster);
    }

    return current;
}

/**
 *  Callback function to be given to traverse_map to find the nearest
 *  free region to a given cluster.
 *
 *  If candidate is closer to cmp than current, the return value will be
 *  candidate, otherwise this function returns current.
 */
    PRIVATE struct free_region **
nearest (current, candidate, cmp)
    struct free_region **current;   // current nearest region.
    struct free_region **candidate; // possible closer region.
    fat_cluster_t cmp;              // cluster to match to.
{
    if (distance (*candidate, cmp) < distance (*current, cmp))
    {
        return candidate;
    }
    else
    {
        return current;
    }
}

/**
 *  Callback function to be given to traverse_map to find the largest
 *  free region by linear search of the in-memory free space map.
 *
 *  This function compares the length of current and candidate, and
 *  returns the region with the largest length.
 */
    PRIVATE struct free_region **
largest (current, candidate, cmp)
    struct free_region **current;   // current max.
    struct free_region **candidate; // next region being compared.
    fat_cluster_t cmp;              // unused.
{
    if ((*candidate)->length > (*current)->length)
    {
        return candidate;
    }
    else
    {
        return current;
    }
}

/**
 *  calculate the closest distance from a free region to a given cluster.
 */
    PRIVATE unsigned int
distance (region, cluster)
    struct free_region *region;     // free region concerned.
    fat_cluster_t cluster;          // cluster to find distance to.
{
    // the cluster could be on either the left or the right of the free
    // region.
    if (cluster < region->start)
    {
        // on the left. Return distance from the cluster to the start
        // of the free region.
        return region->start - cluster;
    }
    else
    {
        // on the right. Return distance from the end of the free region
        // to the cluster.
        return cluster - (region->start + region->length);
    }
}

/**
 *  Given an index for a newly released cluster, and pointers to the free
 *  region structs for the free regions directly to the left and/or right
 *  of that cluster, merge the freed cluster into the free space map.
 */
    PRIVATE void
merge_free_regions (left, right, released)
    struct free_region **left;      // left bordering free region.
    struct free_region **right;     // and the right.
    fat_cluster_t released;         // freed cluster.
{
    // if there is no left bordering free region, the freed cluster is 
    // before the first free region.
    if ((*left == NULL) && (*right != NULL))
    {
        *right = merge_region (*right, released);
    }

    // if the released cluster sits between two free regions...
    if ((*left != NULL) && (*right != NULL))
    {
        // Check if the freed cluster is immediately before the start of
        // the right free region.
        if (released == ((*right)->start - 1))
        {
            // yes. All we have to do, then, is change the start of the
            // right free region to the freed cluster.
            (*right)->start = released;
        }
        else
        {
            // merge with the left free region.
            *left = merge_region (*left, released);
        }
    }

    // if there is no free region on the right, then the freed cluster is
    // after the very last free region.
    if ((*left != NULL) && (*right == NULL))
    {
        *left = merge_region (*left, released);
    }
}

/**
 *  Merge a free region with a nearby newly released cluster. If possible,
 *  this function will simply change the free region descriptor, provided
 *  that the freed cluster is adjacent to the free region; otherwise, a
 *  new free region of size 1 cluster is created and linked either before
 *  or after the existing free region.
 *
 *  Return value is a pointer to the new free region, which may be a list
 *  with two items. The tail of this list will be correctly set to point
 *  to any items on the end of the previous list.
 */
    PRIVATE struct free_region *
merge_region (reg, released)
    struct free_region *reg;        // pointer to nearest free region.
    fat_cluster_t released;         // cluster index that has been freed.
{
    struct free_region *new_reg = reg;

    // first, check if the released cluster forms a new free region.
    if ((released < (reg->start - 1)) || (released > 
          (reg->start + reg->length)))
    {
        // create a new free region descriptor.
        new_reg = safe_malloc (sizeof (struct free_region));
        new_reg->start = released;
        new_reg->length = 1;

        // check if we need to link the new region before or after reg.
        if (released > (reg->start + reg->length))
        {
            // link new_reg after reg.
            new_reg->next = reg->next;
            reg->next = new_reg;
        }
        else
        {
            // link before.
            new_reg->next = reg;
        }
    }

    // is the released cluster adjacent to the start of the free region?
    // If so, move the start of reg to include the freed cluster.
    if (released == (reg->start - 1))
        reg->start -= 1;

    // and likewise for a freed cluster adjacent to the end of the free
    // region.
    if (released == (reg->start + reg->length))
        reg->length += 1;

    // regardless of whether or not a new region was created, new region
    // will always point to the updated state of the free space map.
    return new_reg;
}



// vim: ts=4 sw=4 et
