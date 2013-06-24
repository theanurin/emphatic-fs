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

// mark a given cluster as being unallocated.
extern void release_cluster (fat_cluster_t c);

// Create a new file, and allocate a single cluster to it. The new cluster
// will be marked in the FAT with the end of chain sentinel.
extern int fat_create (fat_file_t *newfd);


#endif // MFATIC_FAT_ALLOC_H

// vim: ts=4 sw=4 et
