/**
 *  create.h
 *
 *  Exports procedures for creating and deleting files.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_CREATE_H
#define MFATIC_CREATE_H

#include "fat.h"


extern int fat_create (const char *path, fat_attr_t attributes);
extern int fat_rename (const char *oldpath, const char *newpath);
extern int fat_unlink (const char *path);
extern void fat_release (fat_file_t *fd);


#endif // MFATIC_CREATE_H

// vim: ts=4 sw=4 et
