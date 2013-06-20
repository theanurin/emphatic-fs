/**
 *  table.h
 *
 *  Declarations of procedures for working with file allocation table
 *  (FAT) entries.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_TABLE_H
#define MFATIC_TABLE_H


// this should be called once, at mount time, to initialise a pointer
// to the volume structure.
extern void table_init (fat_volume_t *volume);

// These routines fetch or write to given cells in the file allocation
// table. Write through LRU caching is implemented internally to avoid large
// overheads for small IO operations.
extern fat_entry_t get_fat_entry (fat_entry_t entry);
extern void put_fat_entry (fat_entry_t entry, fat_entry_t val);


#endif // MFATIC_TABLE_H

// vim: ts=4 sw=4 et
