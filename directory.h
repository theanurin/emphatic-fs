/**
 *  directory.h
 *
 *  Exports procedures for directory operations, such as looking up the
 *  directory entry for a particular file, or listing all files in a given
 *  directory.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_DIRECTORY_H
#define MFATIC_DIRECTORY_H

// needed for definitions of volume and direntry structs.
#include "fat.h"


// look up the directory entry for a particular file.
extern int fat_lookup_dir ( fat_volume_t *v, const char *path, 
  fat_direntry_t *d );

#endif // MFATIC_DIRECTORY_H
