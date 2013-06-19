/**
 *  fileio.c
 *
 *  Implementations for procedures declared in fileio.h.
 *
 *  Author: Matthew Signorini
 */


#include <sys/types.h>
#include <unistd.h>

#include "const.h"
#include "utils.h"
#include "fat.h"
#include "fileio.h"


// This data structure will be used to keep track of how many clients
// have a given file open, and prevent having duplicate fat_file_t's.
struct file_table_entry
{
    fat_file_t                  *file;
    unsigned int                refcount;
    struct file_table_entry     *next;
};


// local function declarations.
PRIVATE off_t set_offset ( fat_file_t *fd, off_t newoff );
PRIVATE void update_current_cluster ( fat_file_t *fd );
PRIVATE size_t count_clusters ( fat_volume_t *v, size_t nbytes );
PRIVATE size_t do_io ( fat_file_t *fd, size_t nbytes, void *buffer,
  size_t ( *safe_io ) ( int, void *, size_t ) );

// These functions maintain the list of currently open files. flist_add
// adds a new file to the list, flist_lookup_file checks if a file is
// already open, and flist_unlink is called when a file is closed, and
// decrements the reference count, removing the file's entry from the
// list when the count reaches 0.
PRIVATE void flist_add ( fat_file_t *fd );
PRIVATE bool flist_lookup_file ( fat_file_t **fd, fat_entry_t inode );
PRIVATE void flist_unlink ( fat_file_t *fd );


// global list of files that are currently open.
PRIVATE struct file_table_entry *files_list;

// pointer to the global volume information. This will be set by a call to
// fileio_init at mount time.
PRIVATE fat_volume_t *volume_info;


/**
 *  This should be called once at mount time, with a pointer to the volume
 *  info structure, which will be stored.
 */
    PUBLIC void
fileio_init ( v )
    fat_volume_t *v;    // pointer to volume info for the mounted fs.
{
    volume_info = v;
}

/**
 *  lookup a file's directory entry, and fill in a file struct with
 *  information about that file, including its cluster list. The struct
 *  must have the volume and mode fields filled in.
 *
 *  Return value is 0 on success, or a negative errno on failure.
 */
    PUBLIC int
fat_open ( path, fd )
    const char *path;   // path from mount point to the file.
    fat_file_t **fd;    // pointer to a file struct pointer to fill in.
{
    fat_direntry_t dir;
    int retval;
    fat_entry_t FAT [ BUF_SIZE ];
    off_t fat_offset, fat_segment, next_segment;
    fat_entry_t this_cluster;
    cluster_list_t **next_item;

    // obtain the directory entry corresponding to the file being opened.
    if ( ( retval = fat_lookup_dir ( volume_info, path, &dir ) ) != 0 )
        return retval;

    // check to see if the file is already open. If so, flist_lookup_file
    // will store the pointer to *fd, and increment the references field,
    // which completes the open() routine.
    if ( flist_lookup_file ( fd, DIR_CLUSTER_START ( &dir ) ) == true )
        return 0;

    // allocate space for the name string. Append a null byte to the end
    // to mark the end of the string, in case the filename takes up all 11
    // chars.
    ( *fd )->name = safe_malloc ( sizeof ( char ) * DIR_NAME_LEN + 1 );
    strncpy ( ( *fd )->name, dir.fname, DIR_NAME_LEN );
    ( *fd )->name [ DIR_NAME_LEN ] = '\0';

    // read the chain of cluster addresses from the file allocation table
    // on the disk, and store them in a linked list in memory, to minimise
    // seek operations on the disk later on. 
    this_cluster = DIR_CLUSTER_START ( &dir );
    next_item = &( ( *fd )->clusters );

    while ( IS_LAST_CLUSTER ( this_cluster ) == false )
    {
        // link a new item into the clusters list.
        *next_item = safe_malloc ( sizeof ( cluster_list_t ) );
        ( *next_item )->cluster_id = this_cluster;
        ( *next_item )->next = NULL;
        next_item = &( ( *next_item )->next );

        // The allocation table cell at index this_cluster contains the
        // index for the next cluster in the chain.
        this_cluster = get_fat_entry ( this_cluster );
    }

    // store the file size, and set the current offset to 0.
    ( *fd )->size = ( size_t ) dir.size;
    ( *fd )->offset = 0;
    ( *fd )->current_cluster = ( *fd )->clusters;

    // add the newly opened file to the open files list.
    flist_add ( *fd );

    return 0;
}

/**
 *  Release the memory allocated to a file structure.
 */
    PUBLIC int
fat_close ( fd )
    fat_file_t *fd;     // pointer to file struct of file being closed.
{
    flist_unlink ( fd );

    // XXX: <oldcode>
    cluster_list_t *prev = NULL;

    // clear the size and offset fields.
    fd->size = 0;
    fd->offset = 0;

    // release the buffer allocated for the filename.
    safe_free ( &( fd->name ) );

    // step through the cluster list, releasing the items as we go.
    for ( cluster_list_t *cp = fd->clusters; cp != NULL; cp = cp->next )
    {
        // We can't just free cp, because then the loop update statement
        // would be accessing memory that has been freed, so we will keep
        // a variable prev which points to the item behind the current
        // item. This *can* safely be freed, except on the first iteration,
        // when there is no previous item; so we don't free anything.
        if ( prev != NULL )
            safe_free ( &prev );

        // set up prev for the next iteration.
        prev = cp;
    }

    safe_free ( &prev );
    // XXX: </oldcode>

    return 0;
}

/**
 *  Read data from an open file.
 */
    PUBLIC size_t
fat_read ( fd, buffer, nbytes )
    fat_file_t *fd;     // file descriptor to read from.
    void *buffer;       // buffer to store bytes read.
    size_t nbytes;      // number of bytes to read.
{
    size_t total_read;

    // transfer clusters from the volume to the buffer using safe_read.
    total_read = do_io ( fd, nbytes, buffer, &safe_read );

    return total_read;
}

/**
 *  Write bytes to a file. If neccessary, additional clusters will be
 *  allocated to the file to accomodate the data being written.
 */
    PUBLIC size_t
fat_write ( fd, buffer, nbytes )
    fat_file_t *fd;     // file descriptor to write to.
    const void *buffer; // data to write.
    int nbytes;         // no of bytes to be written.
{
    size_t cluster_size = CLUSTER_SIZE ( volume_info );
    size_t nr_clusters = count_clusters ( volume_info, fd->size );
    size_t total_written, alloc_bytes;

    // will this write operation go past EOF? If so, we will have to
    // allocate additional clusters to accomodate the data to be written.
    if ( ( fd->offset + ( off_t ) nbytes ) >= ( off_t )( nr_clusters * 
          cluster_size ) )
    {
        // work out how many new clusters we need. Fairly simple maths,
        // but remember that we need to allocate a cluster for a partial
        // fill.
        alloc_bytes = ( fd->offset + ( off_t ) nbytes ) - ( off_t )(
          nr_clusters * cluster_size );

        // allocate new clusters.
        alloc_clusters ( fd, count_clusters ( volume_info, alloc_bytes ) );
    }

    // transfer clusters from the buffer to the volume, using safe_write.
    total_written = do_io ( fd, nbytes, buffer, &safe_write );

    return total_written;
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
    fat_file_t *fd;     // file descriptor.
    off_t offset;       // offset to seek to.
    int whence;         // SEEK_SET, SEEK_CUR or SEEK_END.
{
    off_t retval;

    switch ( whence )
    {
    case SEEK_SET:
        // change offset to offset bytes from the start of the file.
        retval = set_offset ( fd, offset );
        break;

    case SEEK_CUR:
        // add offset to the current file offset.
        retval = set_offset ( fd, fd->offset + offset );
        break;

    case SEEK_END:
        // The offset parameter is presumably <= 0 in this case, because
        // if not, they are seeking past EOF, which is not allowed.
        retval = set_offset ( fd, ( fd->size - 1 ) + offset );
        break;

    default:
        return -EINVAL;
    }

    // update the file descriptor's current cluster field.
    update_current_cluster ( fd );

    return retval;
}

/**
 *  Set a new file offset, checking that the new value is within the 
 *  seekable range. Return value is the new offset if successful, or a
 *  negative errno on failure.
 */
    PRIVATE off_t
set_offset ( fd, newoff )
    fat_file_t *fd;     // file descriptor concerned.
    off_t newoff;       // what to change the offset to.
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

/**
 *  Set the current cluster field of the file descriptor given as a param
 *  to point to the cluster list entry corresponding to the current file
 *  offset (as stored in the file descriptor).
 */
    PRIVATE void
update_current_cluster ( fd )
    fat_file_t *fd;     // file descriptor to be updated.
{
    cluster_list_t *cp;
    off_t i;

    // step through the first n clusters in the cluster list, where n is
    // the number of clusters before the file offset.
    for ( i = 0, cp = fd->clusters;
      ( i != fd->offset / CLUSTER_SIZE ( volume_info ) ) && ( cp != NULL );
      i += 1, cp = cp->next )
    {
        ;
    }

    // cp now points to the cluster where subsequent read or write 
    // operations should start.
    fd->current_cluster = cp;
}

/**
 *  Count how many clusters are required to store a certain number of
 *  bytes. Return value is the number of clusters.
 */
    PRIVATE size_t
count_clusters ( v, nbytes )
    fat_volume_t *v;    // volume descriptor. needed for cluster size.
    size_t nbytes;      // count of bytes being stored.
{
    size_t clusters = nbytes / CLUSTER_SIZE ( v );

    // Note that integer division truncates down. This means that if the
    // buffer partially fills a cluster at the end, we need to add one to
    // this count.
    clusters += ( ( nbytes % CLUSTER_SIZE ( v ) ) != 0 ) ? 1 : 0;

    return clusters;
}

/**
 *  This function carries out a read or write operation on a file on a
 *  FAT file system. Take note: the fourth parameter is a pointer to an
 *  IO function (read or write), which must have the same declaration as
 *  safe_read or safe_write.
 */
    PRIVATE size_t
do_io ( fd, nbytes, buffer, safe_io )
    fat_file_t *fd;     // file handle.
    size_t nbytes;      // number of bytes to transfer.
    void *buffer;       // buffer to read from/write to.
    size_t ( *safe_io ) ( int, void *, size_t );
{
    size_t cluster_size = CLUSTER_SIZE ( volume_info ), block;
    size_t total_bytes = 0
    cluster_list *this_cluster = fd->current_cluster;

    // The first chunk of data to transfer will be either the remaining
    // length in the current cluster, or nbytes, whichever is smaller.
    block = ( nbytes <= ( cluster_size - fd->offset % cluster_size ) ) ?
        nbytes : ( cluster_size - fd->offset % cluster_size );

    // transfer data cluster by cluster. Note that if nbytes is less than
    // one cluster, this may transfer just a single block.
    while ( ( nbytes > 0 ) && ( this_cluster != NULL ) )
    {
        // seek to the correct offset within the correct cluster, as 
        // defined by the file offset.
        safe_seek ( volume_info->dev_fd, 
          CLUSTER_OFFSET ( fd->v, this_cluster ) + 
          ( fd->offset % cluster_size ), SEEK_SET );
        safe_io ( volume_info->dev_fd, buffer, block );

        // update variables to track how much we still have to transfer.
        nbytes -= block;
        buffer += block;
        total_bytes += block;

        // update the file offset.
        fd->offset += block;

        // the next chunk to be transferred is either a full cluster, or
        // nbytes, whichever is smaller.
        block = ( nbytes <= cluster_size ) ? nbytes : cluster_size;

        // step to the next cluster.
        this_cluster = this_cluster->next;
    }

    // update the current cluster field in the file descriptor, if
    // necessary.
    update_current_cluster ( fd );

    return total_bytes;
}

/**
 *  Add a new file to the open files list, setting the ref count to 1.
 *  This should ONLY be called after checking that the file is not already
 *  open, using flist_lookup_file.
 */
    PRIVATE void
flist_add ( fd )
    fat_file_t *fd;     // file descriptor to add.
{
    struct file_table_entry *ft = safe_malloc ( sizeof ( 
          struct file_table_entry ) );

    // fill in the new open file struct. New entries are added to the
    // head of the list, as this saves having to traverse the list to
    // find the tail.
    ft->file = fd;
    ft->refcount = 1;
    ft->next = files_list;

    files_list = ft;
}

/**
 *  check to see if a given file is already open. For the purposes of this
 *  procedure, files are uniquely identified using the index of their first
 *  cluster. This index is referred to as an "i-node" because Emphatic
 *  returns that value in the reply to a stat call.
 *
 *  Return value is true if an open file entry was found, and false if
 *  not.
 */
    PRIVATE bool
flist_lookup_file ( fd, inode )
    fat_file_t **fd;    // If an entry is found, *fd will be filled in.
    fat_entry_t inode;  // unique identifier, as described above.
{
    struct file_table_entry *ft;

    // step through the list of open files, comparing inode values.
    for ( ft = files_list; ( ft != NULL ) && ( ft->file->inode != inode );
      ft = ft->next )
    {
        ;
    }

    // if ft points to a list entry, we have found a match.
    if ( ft != NULL )
    {
        // match found. Store the file handle at *fd, and increment the
        // reference count.
        *fd = ft->file;
        ft->refcount += 1;
        return true;
    }

    // if we reach this point, no open file was found.
    return false;
}

/**
 *  Remove exactly one reference to a file. If there are other references,
 *  this will only decrement the refcount, if this is the last reference,
 *  the file will be removed from the open files list.
 */
    PRIVATE void
flist_unlink ( fd )
    fat_file_t *fd;     // file handle to un reference.
{
    struct file_table_entry **ft, *temp;

    // step through the open files list until we find an item with the
    // same file handle.
    for ( ft = &files_list; ( ( *ft )->file != fd ) && ( *ft != NULL );
      ft = &( ( *ft )->next ) )
    {
        ;
    }

    // if there are other references remaining, don't remove the file
    // from the open files list.
    if ( ( ( *ft )->refcount -= 1 ) != 0 )
        return;

    // no references remain, so unlink this item from the files list.
    temp = *ft;
    *ft = ( *ft )->next;
    safe_free ( &temp );

    // free the file structure.
}


// vim: ts=4 sw=4 et
