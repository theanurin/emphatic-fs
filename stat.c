/**
 *  stat.c
 *
 *  Procedures for obtaining file metadata from the directory entry for
 *  the file concerned.
 *
 *  TODO: maybe implement some sort of caching to make the FUSE getattr
 *  method as efficient as possible? Probably best to do some profiling
 *  first, to see if there is even a problem in this area.
 *
 *  Author: Matthew Signorini
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mfatic-config.h"
#include "const.h"
#include "utils.h"
#include "fat.h"
#include "dostimes.h"
#include "stat.h"


// global pointer to information about the mounted volume.
PRIVATE const fat_volume_t *volume_info;


/**
 *  Initialise the pointer to information about the mounted volume.
 *  This should be called once at mount time.
 */
    PUBLIC void
stat_init (info)
    const fat_volume_t *info;
{
    volume_info = info;
}

/**
 *  Extract a file's metadata from it's directory entry.
 */
    PUBLIC void
unpack_attributes (entry, buffer)
    const fat_direntry_t *entry;    // dir entry to extract info from.
    struct stat *buffer;            // place to store info.
{
    blkcnt_t nr_blocks;
    mode_t file_mode;

    // we will use the cluster size as the block size. Calculate how
    // many clusters are allocated to this file.
    nr_blocks = entry->size / CLUSTER_SIZE (volume_info);

    // if the end of the file partially fills a cluster, we need to
    // increment the count, because division truncates towards zero.
    if ((entry->size % CLUSTER_SIZE (volume_info)) != 0)
        nr_blocks += 1;

    // now we need to put together UNIX style mode information. This is
    // a bit tricky, because FAT does not have a concept of file owner;
    // every file on the volume is accessible to any user.
    //
    // We will translate mode information by giving user group and other
    // the same read/execute permissions, but only user can write, unless
    // the file has the read only attribute, in which case no one can
    // write to it.
    //
    // clear all the mode bits and set permissions.
    file_mode &= 0;
    file_mode |= (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    // Every file must be either a directory or a regular file.
    if ((entry->attributes & ATTR_DIRECTORY) != 0)
    {
        // directory.
        file_mode |= S_IFDIR;
    }
    else
    {
        // regular file.
        file_mode |= S_IFREG;
    }

    // if the file has the read only attrubute set, remove any write
    // permissions.
    if ((entry->attributes & ATTR_READ_ONLY) != 0)
    {
        file_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    }

    // store all the information in the stat structure.
    buffer->st_ino = DIR_CLUSTER_START (entry);
    buffer->st_mode = file_mode;
    buffer->st_nlink = 1;
    buffer->st_size = entry->size;
    buffer->st_blksize = CLUSTER_SIZE (volume_info);
    buffer->st_blocks = nr_blocks;

    // translate DOS time stamps to UNIX time.
    buffer->st_atime = unix_time (entry->access_date, 0);
    buffer->st_mtime = unix_time (entry->write_date, entry->write_time);
}


// vim: ts=4 sw=4 et
