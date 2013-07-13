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
extern int fat_lookup_dir (const char *path, fat_direntry_t *buffer,
  fat_file_t **parent, unsigned int *index);

// read or write a given entry from a given directory.
extern void get_directory_entry (fat_direntry_t *buffer, 
  fat_entry_t inode, unsigned int index);
extern void put_directory_entry (const fat_direntry_t *buffer,
  fat_entry_t inode, unsigned int index);

// Add a new directory to the active directories list.
extern fat_entry_t add_parent_dir (fat_file_t *parent);

// get a file handle for a given parent directory.
extern fat_file_t * get_parent_fd (fat_entry_t inode);

// release a file's parent dir. This should be called when the file is
// being closed.
extern void release_parent_dir (fat_entry_t inode);

// procedures to delete a directory entry, or write a new entry.
extern void dir_delete_entry (const fat_file_t *dirfd, unsigned int index);
extern void dir_write_entry (fat_file_t *dirfd, 
  const fat_direntry_t *entry);

// store a value in a directory entry's start cluster field.
extern void put_direntry_cluster (fat_direntry_t *entry, fat_cluster_t val);


#endif // MFATIC_DIRECTORY_H

// vim: ts=4 sw=4 et
