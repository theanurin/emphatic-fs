/**
 *  fileio.h
 *
 *  Declares procedures for generic file operations on an Emphatic file
 *  system, such as read, write and open. These procedures will also work
 *  on directories, and treat them the same as an ordinary file, allowing
 *  the raw directory table to be read and modified.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_FILEIO_H
#define MFATIC_FILEIO_H


// this is needed for type definitions for fat_file_t.
#include "fat.h"


// called at mount time, with a pointer to the volume info struct for the
// file system being mounted.
extern void fileio_init (const fat_volume_t *v);

// locate a file on a device, and fill in the file struct pointed to by the
// second param. The file struct must have the volume and mode fields filled
// in before the call to this procedure.
extern int fat_open (const char *path, fat_file_t *fd);
extern int fat_close (fat_file_t *fd);

// read and write from a file.
extern size_t fat_read (fat_file_t *fd, void *buf, size_t nbytes);
extern size_t fat_write (fat_file_t *fd, const void *buf, size_t nbytes);

// change the current position in a file.
extern off_t fat_seek (fat_file_t *fd, off_t offset, int whence);


#endif // MFATIC_FILEIO_H

// vim: ts=4 sw=4 et
