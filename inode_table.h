/**
 *  inode_table.h
 *
 *  Provides type definitions and procedure declarations for handling
 *  a list of active files, indexed using a unique numerical key, referred
 *  to as an i-node. Because FAT does not use i-nodes like UNIX does, we
 *  will typically use the index of the first cluster of a file, as that
 *  quantity satisfies the properties of being unique, and applicable to
 *  all files.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_INODE_TABLE_H
#define MFATIC_INODE_TABLE_H

// this is needed for the fat_* type defs.
#include "fat.h"


// We will use a linked list to keep track of active files, which has
// the advantage of not being restricted by a fixed number of slots in
// a statically allocated array. Be aware that the fat_file_t struct
// has a field for the i-node, so we will use that, and avoid duplicating
// data.
typedef struct inode_entry
{
    fat_file_t              *file;
    unsigned int            refcount;
    struct inode_entry      *next;
}
file_list_t;

// macro for unpacking the i-node field from the fat_file_t structure.
#define INODE(entry)    ((entry)->file->inode)


// Procedures for adding items, looking up an item matching a given key,
// and removing items, from an inode list.
extern void ilist_add (file_list_t **list, fat_file_t *fd);
extern bool ilist_lookup_file (file_list_t **list, fat_file_t **fd, 
  fat_entry_t inode);
extern void ilist_unlink (file_list_t **list, fat_entry_t inode);


#endif // MFATIC_INODE_TABLE_H

// vim: ts=4 sw=4 et
