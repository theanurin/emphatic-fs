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


// need type definitions for fat_*
#include "fat.h"


// initialise the map of the free space on a given volume.
extern void init_clusters_map (const fat_volume_t *v);

// provide general usage statistics.
extern int used_clusters (void);
extern int free_clusters (void);

// Find the unallocated cluster closest to near, and mark it as allocated.
// return value is the cluster index of the cluster that was allocated.
// This function will also modify the FAT, such that the newly allocated
// cluster is marked with the end of chain sentinel, and near's entry
// points to the newly allocated cluster.
extern fat_cluster_t new_cluster (fat_cluster_t near);

// Allocate a cluster for a new file. In this case, the allocated cluster
// will be in the middle of the largest contiguous region of free space,
// ensuring that all files have maximal room to grow.
extern fat_cluster_t fat_alloc_node (void);

// mark a given cluster as being unallocated.
extern void release_cluster (fat_cluster_t c);


#endif // MFATIC_FAT_ALLOC_H

// vim: ts=4 sw=4 et
