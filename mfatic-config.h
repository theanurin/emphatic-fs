/**
 *  mfatic-config.h
 *
 *  Defines feature test macros, and imports important system types
 *  which are used throughout Emphatic
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_CONFIG_H
#define MFATIC_CONFIG_H

#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <stddef.h>

// build Emphatic to work with FAT32 file systems. At present, we do not
// support FAT12/16.
#define MFATIC_32

// Number of FAT sectors to keep in the LRU cache.
#define CACHE_SECTORS_MAX           128

// copyright string to print with version info.
#define COPYRIGHT_STR               \
    "Copyright (c) 2013 Matthew Signorini <emphatic-fs@googlecode.com>"

#endif // MFATIC_CONFIG_H

// vim: ts=4 sw=4 et
