/**
 *  mfatic-fuse.c
 *
 *  Fuse daemon for mounting an Emphatic file system, and methods for
 *  carrying out file operations on an Emphatic fs.
 *
 *  Author: Matthew Signorini
 */

#include <fuse.h>


#include "const.h"
#include "utils.h"
#include "fat.h"
#include "fileio.h"


// Declarations for methods to handle file operations on an mfatic file
// system.
//
// Open, read and write are pretty self explanatory. release is called
// when a file is being closed. The struct fuse_file_info parameter contains
// a "file handle" which is set by open() and used by the other operations
// to find the struct used to represent the file.
PRIVATE int mfatic_open (const char *name, struct fuse_file_info *fd);
PRIVATE int mfatic_read (const char *name, char *buf, size_t nbytes, 
  off_t off, struct fuse_file_info *fd);
PRIVATE int mfatic_write (const char *name, const char *buf, size_t nbytes,
  off_t off, struct fuse_file_info *fd);
PRIVATE int mfatic_release (const char *name, struct fuse_file_info *fd);

// get file or file system metadata.
PRIVATE int mfatic_getattr (const char *name, struct stat *st);
PRIVATE int mfatic_statfs (const char *name, struct statvfs *st);

// check if a file can be accessed for mode.
PRIVATE int mfatic_access (const char *path, int mode);

// create and delete directories and regular files.
PRIVATE int mfatic_mknod (const char *name, mode_t mode, dev_t dev);
PRIVATE int mfatic_mkdir (const char *name, mode_t mode);
PRIVATE int mfatic_unlink (const char *name);
PRIVATE int mfatic_rmdir (const char *name);

// read the contents of a directory. Opening and closing a directory is
// done using mfatic_open and mfatic_release, because a directory is,
// after all, just a file.
PRIVATE int mfatic_readdir (const char *path, void *buf, fuse_fill_dir_t
  *filler, off_t offset, struct fuse_file_info *fd);

// rename a file
PRIVATE int mfatic_rename (const char *old, const char *new);

// truncate a file to length bytes.
PRIVATE int mfatic_truncate (const char *path, off_t length);

// change the access and/or modification times of a file.
PRIVATE int mfatic_utime (const char *path, const struct utimbuf *time);


// Pointer to a struct containing information about the mounted file system
PRIVATE fat_volume_t *volume_info;


// This struct is used by the main loop in the FUSE library to dispatch
// to our methods for handling operations on an mfatic fs, such as open,
// read write and so on.
PRIVATE struct fuse_operations mfatic_ops =
{
    .getattr    = mfatic_getattr,
    .access     = mfatic_access,
    .opendir    = mfatic_open,
    .readdir    = mfatic_readdir,
    .releasedir = mfatic_release,
    .mknod      = mfatic_mknod,
    .mkdir      = mfatic_mkdir,
    .unlink     = mfatic_unlink,
    .rmdir      = mfatic_rmdir,
    .rename     = mfatic_rename,
    .truncate   = mfatic_truncate,
    .utime      = mfatic_utime,
    .open       = mfatic_open,
    .read       = mfatic_read,
    .write      = mfatic_write,
    .release    = mfatic_release,
    .statfs     = mfatic_statfs,
};


/**
 *  Handle a request to open the file at the absolute path (on our device)
 *  given by the first parameter. This routine creates a new file handle,
 *  and stores a pointer to it in the struct pointed to by the second
 *  parameter. This struct (and thus a pointer to the file handle) is
 *  passed to our read and write handlers to service subsequent operations.
 *
 *  Return value is 0 on success, or a negative errno value.
 */
    PRIVATE int
mfatic_open (path, fd)
    const char *path;           // path from mount point to the file.
    struct fuse_file_info *fd;
{
    // allocate memory for the file handle.
    fat_file_t *newfile;
    int retval;

    // open the file. If it fails, return an error, and release the now
    // unneeded file struct back to the pool of free memory.
    if ((retval = fat_open (path, &newfile)) != 0)
    {
        return retval;
    }

    // save a pointer to the file struct.
    fd->fh = newfile;

    return 0;
}

/**
 *  This method is called when a file is being closed. It releases the
 *  memory allocated to the file handle struct.
 *
 *  Fuse ignores the return value of this method.
 */
    PRIVATE int
mfatic_release (path, fd)
    const char *path;           // absolute path. Unused.
    struct fuse_file_info *fd;
{
    fat_file_t *oldfd = (fat_file_t *) fd->fh;

    // release the memory allocated to the file struct.
    fat_close (oldfd);
    
    return 0;
}

/**
 *  Read nbytes from a file, starting at offset bytes from the start, and
 *  store them in buf.
 *
 *  Return value is the number of bytes read, or EOF.
 */
    PRIVATE int
mfatic_read (name, buf, nbytes, offset, fd)
    const char *name;           // file name. Unused.
    char *buf;                  // buffer to store data read.
    int nbytes;                 // no of bytes to read.
    off_t offset;               // where to start reading.
    struct fuse_file_info *fd;  // file handle.
{
    fat_file_t *rf = (fat_file_t *) fd->fh;

    // seek to the start offset requested.
    if (fat_seek (rf, offset, SEEK_SET) != offset)
        return EOF;

    // read the data.
    return fat_read (rf, buf, nbytes);
}

/**
 *  write nbytes from buf to a file, starting at offset bytes into the
 *  file.
 *
 *  Return value is the number of bytes written, or EOF, if the offset is
 *  past the end of the file.
 */
    PRIVATE int
mfatic_write (name, buf, nbytes, offset, fd)
    const char *name;           // file name. Not used.
    const char *buf;            // data to write to the file.
    int nbytes;                 // length of the buffer.
    off_t offset;               // where to start writing.
    struct fuse_file_info *fd;  // file handle.
{
    fat_file_t *wf = (fat_file_t *) fd->fh;

    // seek to the offset at which to begin writing.
    if (fat_seek (wf, offset, SEEK_SET) != offset)
        return EOF;

    // write the data.
    return fat_write (wf, buf, nbytes);
}

/**
 *  fetch attribute information about a given file, including an "i-node"
 *  number (which is a unique identifier for the file; we will use the
 *  cluster index of the file's first cluster, as every file has one, and
 *  each file must start at a different cluster), file size, and time
 *  stamps.
 *
 *  Return value is 0 on success or a negative errno on error.
 */
    PRIVATE int
mfatic_getattr (path, stat)
    const char *path;       // file to get information on.
    struct stat *st;        // buffer to store the information.
{
    fat_direntry_t file_info;
    int retval;

    // fetch the directory entry for the file.
    if ((retval = fat_lookup_dir (path, &file_info)) != 0)
        return retval;

    // extract the file's metadata from the directory entry.
    unpack_attributes (&file_info, st);

    return 0;
}

/**
 *  Return information about a mounted FAT file system.
 *
 *  Return value is 0 on success, this call cannot fail.
 */
    PRIVATE int
mfatic_statfs (name, st)
    const char *name;       // file name within the fs. Not used.
    struct statvfs *st;     // buffer to store fs information.
{
    // store the cluster size. Fragments are 1 cluster in size.
    st->f_bsize = CLUSTER_SIZE (volume_info);
    st->f_frsize = st->f_bsize;

    // information on the number of clusters which are allocated or
    // available is gathered at mount time by the free space manager.
    st->f_blocks = used_clusters () + free_clusters ();
    st->f_bfree = free_clusters ();
    st->f_bavail = st->f_bfree;

    // At present, we do not support long file names; only the old 8.3
    // (8 chars, plus 3 char extension) names.
    st->f_namemax = DIR_NAME_LEN;

    return 0;
}

/**
 *  Read information from a directory about the files and/or subdirectories
 *  contained within it.
 */
    PRIVATE int
mfatic_readdir (path, buffer, filler, offset, fd)
    const char *path;               // directory name. Unused.
    void *buffer;                   // buffer to write info to.
    fuse_fill_dir_t *filler;        // function to fill the buffer with.
    off_t offset;                   // index of first direntry to read.
    struct fuse_file_info *fd;      // directory file handle.
{
    fat_direntry_t entry;
    struct stat attrs;

    // move the read offset to the start of the first entry to read.
    if (fat_seek (fd, offset * sizeof (fat_direntry_t), SEEK_SET) != 
      offset * sizeof (fat_direntry_t))
    {
        return EOF;
    }

    // iteratively read entries until the filler function indicates that
    // we have filled the buffer.
    do
    {
        // read the next entry.
        fat_read (fd, &entry, sizeof (fat_direntry_t));

        // unpack file attribute information from the directory entry.
        unpack_attributes (&entry, &attrs);
    }
    while ((*filler) (buffer, entry->fname, &attrs, offset += 1) != 1);

    return 0;
}


// vim: ts=4 sw=4 et
