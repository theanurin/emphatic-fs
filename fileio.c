/**
 *  fileio.c
 *
 *  Implementations for procedures declared in fileio.h.
 *
 *  Author: Matthew Signorini
 */

// this feature test macro tells the system headers to make off_t a 64
// bit number, so that we can use offsets greater that +/- 2 GB.
#define _FILE_OFFSET_BITS   64

#include <sys/types.h>
#include <unistd.h>

#include "const.h"
#include "fat.h"
#include "fileio.h"


/**
 *  lookup a file's directory entry, and fill in a file struct with
 *  information about that file, including its cluster list. The struct
 *  must have the volume and mode fields filled in.
 *
 *  Return value is 0 on success, or a negative errno on failure.
 */
    PUBLIC int
fat_open ( path, fd )
    const char *path;	// path from mount point to the file.
    fat_file_t *fd;	// pointer to the struct to fill in.
{
    fat_direntry_t dir;
    int retval;
    fat_entry_t FAT [ BUF_SIZE ];
    off_t fat_offset, fat_segment, next_segment;
    fat_entry_t this_cluster;
    cluster_list_t **next_cluster = &( fd->clusters );

    // obtain the directory entry corresponding to the file being opened.
    if ( ( retval = fat_lookup_dir ( fd->v, path, &dir ) ) != 0 )
	return retval;

    // store the file size, and set the current offset to 0.
    fd->fsize = ( size_t ) dir.size;
    fd->cur_offset = 0;

    // allocate space for the name string. Append a null byte to the end
    // to mark the end of the string, in case the filename takes up all 11
    // chars.
    fd->name = safe_malloc ( sizeof ( char ) * DIR_NAME_LEN + 1 );
    strncpy ( fd->name, dir.fname, DIR_NAME_LEN );
    fd->name [ DIR_NAME_LEN ] = '\0';

    // read the chain of cluster addresses from the file allocation table
    // on the disk, and store them in a linked list in memory, to minimise
    // seek operations on the disk. Provided the cluster chain is not
    // humungously long, and the file's clusters are localised, this code
    // will be fairly efficient, as our buffering of segments of the FAT
    // requires very few disk accesses if all of the file's FAT entries are
    // near each other.
    this_cluster = ( dir.cluster_msb << 16 ) | ( dir.cluster_lsb );

    while ( IS_LAST_CLUSTER ( this_cluster ) == 0 )
    {
	// link a new item into the clusters list.
	*next_cluster = safe_malloc ( sizeof ( cluster_list_t ) );
	( *next_cluster )->cluster_id = this_cluster;
	( *next_cluster )->next = NULL;
	next_cluster = &( ( *next_cluster )->next );

	// calculate offset into FAT of next entry, in bytes.
	fat_offset = this_cluster * FAT_ENTSIZE;

	// calculate the segment of the FAT where the next entry is. If
	// it is the same segment we have in memory, we don't need to read
	// it from disk again.
	next_segment = fat_offset / BUF_SIZE;

	if ( next_segment != fat_segment )
	{
	    // read the new segment from the disk into our buffer.
	    // first, seek to the correct location on the disk.
	    lseek ( fd->v->dev_fd, 
	      FAT_START ( fd->v ) * SECTOR_SIZE ( fd->v ) + 
	      next_segment * BUF_SIZE * sizeof ( fat_entry_t ), 
	      SEEK_SET );
	    read ( fd->v->dev_fd, FAT, BUF_SIZE * sizeof ( fat_entry_t ) );

	    fat_segment = next_segment
	}

	// offset of the next cluster address into our buffer.
	fat_offset %= BUF_SIZE;

	this_cluster = FAT [ fat_offset ];
    }

    return 0;
}
