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


// tell directory.c where the volume info is.
extern void directory_init (const fat_volume_t *volinfo);

// look up the directory entry for a particular file.
extern int fat_lookup_dir (const char *path, fat_direntry_t *d);

// read or write a given entry from a given directory.
extern void get_directory_entry (fat_direntry_t *buffer, 
  fat_entry_t inode, unsigned int index);
extern void put_directory_entry (const fat_direntry_t *buffer,
  fat_entry_t inode, unsigned int index);


#endif // MFATIC_DIRECTORY_H

// vim: ts=4 sw=4 et
