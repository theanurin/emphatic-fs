/**
 *  fat_alloc.h
 *
 *  Declarations of procedures used for allocating and releasing clusters
 *  by the Emphatic FAT driver.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_FAT_ALLOC_H
#define MFATIC_FAT_ALLOC_H


// Find the unallocated cluster closest to near, and mark it as allocated.
// return value is the cluster index of the cluster that was allocated.
extern fat_cluster_t new_cluster ( fat_volume_t *v, fat_cluster_t near );

// mark a given cluster as being unallocated.
extern void release_cluster ( fat_volume_t *v, fat_cluster_t c );

// Create a new file, and allocate a single cluster to it.
extern int mfatic_create ( fat_volume_t *v, fat_file_t *newfd );


#endif // MFATIC_FAT_ALLOC_H
