/**
 *  fileio.c
 *
 *  Implementations for procedures declared in fileio.h.
 *
 *  Author: Matthew Signorini
 */


#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "mfatic-config.h"
#include "const.h"
#include "utils.h"
#include "fat.h"
#include "inode_table.h"
#include "table.h"
#include "directory.h"
#include "fat_alloc.h"
#include "fileio.h"


// local function declarations.
PRIVATE off_t set_offset (fat_file_t *fd, off_t newoff);
PRIVATE void update_current_cluster (fat_file_t *fd);
PRIVATE size_t count_clusters (const fat_volume_t *v, size_t nbytes);
PRIVATE size_t do_io (fat_file_t *fd, size_t nbytes, void *buffer,
  size_t (*safe_io) (int, void *, size_t));


// global list of files that are currently open.
PRIVATE file_list_t *files_list;

// pointer to the global volume information. This will be set by a call to
// fileio_init at mount time.
PRIVATE const fat_volume_t *volume_info;


/**
 *  This should be called once at mount time, with a pointer to the volume
 *  info structure, which will be stored.
 */
    PUBLIC void
fileio_init (v)
    const fat_volume_t *v;  // pointer to volume info for the mounted fs.
{
    volume_info = v;
}

/**
 *  Given a directory entry, create a new file structure, and read in the
 *  linked list of clusters. A pointer to this structure will be stored at
 *  the location pointed to by the second parameter.
 *
 *  Return value is 0 on success, or a negative errno on failure.
 */
    PUBLIC int
fat_open_fd (entry, inode, index, fd)
    const fat_direntry_t *entry;    // directory entry for the file.
    fat_entry_t inode;              // parent dir inode.
    unsigned int index;             // dir entry index.
    fat_file_t **fd;                // file handle to be filled in.
{
    fat_entry_t this_cluster;
    cluster_list_t **next_item;

    // check to see if the file is already open. If so, ilist_lookup_file
    // will store the pointer to *fd, and increment the references field,
    // which completes the open() routine.
    if (ilist_lookup_file (&files_list, fd, DIR_CLUSTER_START (entry)) 
      == true)
    {
        return 0;
    }

    // allocate a new file struct.
    *fd = safe_malloc (sizeof (fat_file_t));

    // allocate space for the name string. Append a null byte to the end
    // to mark the end of the string, in case the filename takes up all 11
    // chars.
    (*fd)->name = safe_malloc (sizeof (char) * DIR_NAME_LEN + 1);
    strncpy ((*fd)->name, entry->fname, DIR_NAME_LEN);
    (*fd)->name [DIR_NAME_LEN] = '\0';

    // read the chain of cluster addresses from the file allocation table
    // on the disk, and store them in a linked list in memory, to minimise
    // seek operations on the disk later on. 
    this_cluster = DIR_CLUSTER_START (entry);
    next_item = &((*fd)->clusters);

    while (IS_LAST_CLUSTER (this_cluster) == false)
    {
        // link a new item into the clusters list.
        *next_item = safe_malloc (sizeof (cluster_list_t));
        (*next_item)->cluster_id = this_cluster;
        (*next_item)->next = NULL;
        next_item = &((*next_item)->next);

        // The allocation table cell at index this_cluster contains the
        // index for the next cluster in the chain.
        this_cluster = get_fat_entry (this_cluster);
    }

    // store the file size, and set the current offset to 0.
    (*fd)->size = (size_t) entry->size;
    (*fd)->offset = 0;
    (*fd)->current_cluster = (*fd)->clusters;
    (*fd)->attributes = entry->attributes;
    (*fd)->directory_inode = inode;
    (*fd)->dir_entry_index = index;
    (*fd)->refcount = 0;    // this will be incremented by ilist_add.

    // add the newly opened file to the open files list.
    ilist_add (&files_list, *fd);

    return 0;
}

/**
 *  conventional open system call. Takes a path name, and creates a file
 *  handle. Note that this procedure basically does a fat_lookup_dir and
 *  then a fat_open_fd, so cannot be called from either of those functions.
 */
    PUBLIC int
fat_open (path, fd)
    const char *path;       // absolute path to the file to open.
    fat_file_t **fd;        // file handle to fill in.
{
    fat_file_t *pfd;
    unsigned int index;
    fat_direntry_t entry;
    int retval;

    // look up the file's directory entry.
    if ((retval = fat_lookup_dir (path, &entry, &pfd, &index)) != 0)
        return retval;

    // create a file structure.
    if ((retval = fat_open_fd (&entry, pfd->inode, index, fd)) != 0)
    {
        fat_close (pfd);
        return retval;
    }

    // add reference for parent directory.
    add_parent_dir (pfd);

    return 0;
}

/**
 *  Release the memory allocated to a file structure.
 */
    PUBLIC int
fat_close (fd)
    fat_file_t *fd;     // pointer to file struct of file being closed.
{
    if (fd->directory_inode != 0)
        release_parent_dir (fd->directory_inode);

    ilist_unlink (&files_list, fd->inode);

    return 0;
}

/**
 *  Read data from an open file.
 */
    PUBLIC size_t
fat_read (fd, buffer, nbytes)
    fat_file_t *fd;     // file descriptor to read from.
    void *buffer;       // buffer to store bytes read.
    size_t nbytes;      // number of bytes to read.
{
    size_t total_read;

    // transfer clusters from the volume to the buffer using safe_read.
    total_read = do_io (fd, nbytes, buffer, &safe_read);

    return total_read;
}

/**
 *  Write bytes to a file. If neccessary, additional clusters will be
 *  allocated to the file to accomodate the data being written.
 */
    PUBLIC size_t
fat_write (fd, buffer, nbytes)
    fat_file_t *fd;     // file descriptor to write to.
    const void *buffer; // data to write.
    size_t nbytes;      // no of bytes to be written.
{
    size_t cluster_size = CLUSTER_SIZE (volume_info);
    size_t nr_clusters = count_clusters (volume_info, fd->size);
    size_t total_written, alloc_bytes;

    // will this write operation go past EOF? If so, we will have to
    // allocate additional clusters to accomodate the data to be written.
    if ((fd->offset + (off_t) nbytes) >= (off_t) (nr_clusters * 
          cluster_size))
    {
        // work out how many new clusters we need. Fairly simple maths,
        // but remember that we need to allocate a cluster for a partial
        // fill.
        alloc_bytes = (fd->offset + (off_t) nbytes) - (off_t) (nr_clusters *
          cluster_size);

        // allocate new clusters.
        alloc_clusters (fd, count_clusters (volume_info, alloc_bytes));
    }

    // transfer clusters from the buffer to the volume, using safe_write.
    total_written = do_io (fd, nbytes, buffer, &safe_write);

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
fat_seek (fd, offset, whence)
    fat_file_t *fd;     // file descriptor.
    off_t offset;       // offset to seek to.
    int whence;         // SEEK_SET, SEEK_CUR or SEEK_END.
{
    off_t retval;

    switch (whence)
    {
    case SEEK_SET:
        // change offset to offset bytes from the start of the file.
        retval = set_offset (fd, offset);
        break;

    case SEEK_CUR:
        // add offset to the current file offset.
        retval = set_offset (fd, fd->offset + offset);
        break;

    case SEEK_END:
        // The offset parameter is presumably <= 0 in this case, because
        // if not, they are seeking past EOF, which is not allowed.
        retval = set_offset (fd, (fd->size - 1) + offset);
        break;

    default:
        return -EINVAL;
    }

    // update the file descriptor's current cluster field.
    update_current_cluster (fd);

    return retval;
}

/**
 *  Set a new file offset, checking that the new value is within the 
 *  seekable range. Return value is the new offset if successful, or a
 *  negative errno on failure.
 */
    PRIVATE off_t
set_offset (fd, newoff)
    fat_file_t *fd;     // file descriptor concerned.
    off_t newoff;       // what to change the offset to.
{
    if ((newoff >= 0) && (newoff < fd->size))
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
update_current_cluster (fd)
    fat_file_t *fd;     // file descriptor to be updated.
{
    cluster_list_t *cp;
    off_t i;

    // step through the first n clusters in the cluster list, where n is
    // the number of clusters before the file offset.
    for (i = 0, cp = fd->clusters;
      (i != fd->offset / CLUSTER_SIZE (volume_info)) && (cp != NULL);
      i += 1, cp = cp->next)
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
count_clusters (v, nbytes)
    const fat_volume_t *v;    // volume descriptor. needed for cluster size.
    size_t nbytes;      // count of bytes being stored.
{
    size_t clusters = nbytes / CLUSTER_SIZE (v);

    // Note that integer division truncates down. This means that if the
    // buffer partially fills a cluster at the end, we need to add one to
    // this count.
    clusters += ((nbytes % CLUSTER_SIZE (v)) != 0) ? 1 : 0;

    return clusters;
}

/**
 *  This function carries out a read or write operation on a file on a
 *  FAT file system. Take note: the fourth parameter is a pointer to an
 *  IO function (read or write), which must have the same declaration as
 *  safe_read or safe_write.
 */
    PRIVATE size_t
do_io (fd, nbytes, buffer, safe_io)
    fat_file_t *fd;     // file handle.
    size_t nbytes;      // number of bytes to transfer.
    void *buffer;       // buffer to read from/write to.
    size_t (*safe_io) (int, void *, size_t);
{
    size_t cluster_size = CLUSTER_SIZE (volume_info), block;
    size_t total_bytes = 0;
    cluster_list_t *this_cluster = fd->current_cluster;

    // The first chunk of data to transfer will be either the remaining
    // length in the current cluster, or nbytes, whichever is smaller.
    block = (nbytes <= (cluster_size - fd->offset % cluster_size)) ?
        nbytes : (cluster_size - fd->offset % cluster_size);

    // transfer data cluster by cluster. Note that if nbytes is less than
    // one cluster, this may transfer just a single block.
    while ((nbytes > 0) && (this_cluster != NULL))
    {
        // seek to the correct offset within the correct cluster, as 
        // defined by the file offset.
        safe_seek (volume_info->dev_fd, 
          CLUSTER_OFFSET (fd->v, this_cluster) + 
          (fd->offset % cluster_size), SEEK_SET);
        safe_io (volume_info->dev_fd, buffer, block);

        // update variables to track how much we still have to transfer.
        nbytes -= block;
        buffer += block;
        total_bytes += block;

        // update the file offset.
        fd->offset += block;

        // the next chunk to be transferred is either a full cluster, or
        // nbytes, whichever is smaller.
        block = (nbytes <= cluster_size) ? nbytes : cluster_size;

        // step to the next cluster.
        this_cluster = this_cluster->next;
    }

    // update the current cluster field in the file descriptor, if
    // necessary.
    update_current_cluster (fd);

    return total_bytes;
}


// vim: ts=4 sw=4 et
