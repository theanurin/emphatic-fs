/**
 *  stat.h
 *
 *  Procedures for obtaining information about files.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_STAT_H
#define MFATIC_STAT_H


// needed for fat_* type definitions.
#include "fat.h"


// called at mount time with a pointer to information about the volume
// being mounted.
extern void stat_init (const fat_volume_t *info);

// get file metadata from the file's directory entry.
extern void unpack_attributes (const fat_direntry_t *entry, 
  struct stat *buffer);


#endif // MFATIC_STAT_H

// vim: ts=4 sw=4 et
