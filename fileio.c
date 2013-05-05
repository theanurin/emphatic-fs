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


// local function declarations.
PRIVATE off_t set_offset ( fat_file_t *fd, off_t newoff );


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
    fd->size = ( size_t ) dir.size;
    fd->offset = 0;

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
	    safe_seek ( fd->v->dev_fd, 
	      FAT_START ( fd->v ) * SECTOR_SIZE ( fd->v ) + 
	      next_segment * BUF_SIZE * sizeof ( fat_entry_t ), 
	      SEEK_SET );
	    safe_read ( fd->v->dev_fd, FAT, BUF_SIZE * 
	      sizeof ( fat_entry_t ) );

	    fat_segment = next_segment
	}

	// offset of the next cluster address into our buffer.
	fat_offset %= BUF_SIZE;

	this_cluster = FAT [ fat_offset ];
    }

    return 0;
}

/**
 *  Release the memory allocated to a file structure.
 */
    PUBLIC int
fat_close ( fd )
    fat_file_t *fd;	// pointer to file struct of file being closed.
{
    cluster_list_t *prev = NULL;

    // clear the size and offset fields.
    fd->size = 0;
    fd->offset = 0;

    // release the buffer allocated for the filename.
    safe_free ( &( fd->name ) );

    // step through the cluster list, releasing the items as we go.
    for ( cluster_list_t **cpp = &( fd->clusters ); *cpp != NULL;
      cpp = &( ( *cpp )->next ) )
    {
	// We can't just free *cpp, because then the loop update statement
	// would be accessing memory that has been freed, so we will keep
	// a variable prev which points to the item behind the current
	// item. This *can* safely be freed, except on the first iteration,
	// when there is no previous item; so we don't free anything.
	if ( prev != NULL )
	    safe_free ( &prev );

	// set up prev for the next iteration.
	prev = *cpp;
    }

    return 0;
}

/**
 *  Read data from an open file.
 */
    PUBLIC size_t
fat_read ( fd, buffer, nbytes )
    fat_file_t *fd;	// file descriptor to read from.
    void *buffer;	// buffer to store bytes read.
    size_t nbytes;	// number of bytes to read.
{
    cluster_list_t *rc = fd->clusters;
    size_t cluster_size = CLUSTER_SIZE ( fd->v );
    size_t total_read = 0;

    // locate the cluster based on the current offset.
    for ( int i = 0; 
      ( rc != NULL ) && ( i != fd->offset / cluster_size ); 
      i ++, rc = rc->next )
    {
	;
    }

    // initial read block is either nbytes or the number of bytes remaining
    // in the first cluster, whichever is smaller.
    nread = ( nbytes <= ( cluster_size - fd->offset % cluster_size ) ) ?
	nbytes : cluster_size - fd->offset % cluster_size;

    // start reading clusters.
    while ( ( nbytes > 0 ) && ( rc != NULL ) )
    {
	// seek to the cluster, and read.
	safe_seek ( fd->v->dev_fd, CLUSTER_OFFSET ( fd->v, rc ), SEEK_SET );
	safe_read ( fd->v->dev_fd, buffer, nread );

	// update the variables.
	nbytes -= nread;
	buffer += nread;
	total_read += nread;

	// next chunk to read is either nbytes or a full cluster, whichever
	// is smaller.
	nread = ( nbytes <= cluster_size ) ? nbytes : cluster_size;

	// step to the next cluster.
	rc = rc->next;
    }

    return total_read;
}

/**
 *  Change the current offset in a file. Parameters are the same as the
 *  lseek system call.
 *
 *  Return value is either the new offset (positive value) or a negative
 *  errno.
 */
    PUBLIC off_t
fat_seek ( fd, offset, whence )
    fat_file_t *fd;	// file descriptor.
    off_t offset;	// offset to seek to.
    int whence;		// SEEK_SET, SEEK_CUR or SEEK_END.
{
    switch ( whence )
    {
    case SEEK_SET:
	// change offset to offset bytes from the start of the file.
	return set_offset ( fd, offset );
	break;

    case SEEK_CUR:
	// add offset to the current file offset.
	return set_offset ( fd, fd->offset + offset );
	break;

    case SEEK_END:
	// The offset parameter is presumably <= 0 in this case, because
	// if not, they are seeking past EOF, which is not allowed.
	return set_offset ( fd, ( fd->size - 1 ) + offset );
	break;

    default:
	return -EINVAL;
    }
}

/**
 *  Set a new file offset, checking that the new value is within the 
 *  seekable range. Return value is the new offset if successful, or a
 *  negative errno on failure.
 */
    PRIVATE off_t
set_offset ( fd, newoff )
    fat_file_t *fd;	// file descriptor concerned.
    off_t newoff;	// what to change the offset to.
{
    if ( ( newoff >= 0 ) && ( newoff < fd->size ) )
    {
	fd->offset = newoff;
	return newoff;
    }
    else
    {
	return -EINVAL;
    }
}
