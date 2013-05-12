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
PRIVATE int mfatic_open ( const char *name, struct fuse_file_info *fd );
PRIVATE int mfatic_read ( const char *name, char *buf, size_t nbytes, 
  off_t off, struct fuse_file_info *fd );
PRIVATE int mfatic_write ( const char *name, const char *buf, size_t nbytes,
  off_t off, struct fuse_file_info *fd );
PRIVATE int mfatic_release ( const char *name, struct fuse_file_info *fd );

// get file or file system metadata.
PRIVATE int mfatic_getattr ( const char *name, struct stat *st );
PRIVATE int mfatic_statfs ( const char *name, struct statvfs *st );

// check if a file can be accessed for mode.
PRIVATE int mfatic_access ( const char *path, int mode );

// create and delete directories and regular files.
PRIVATE int mfatic_mknod ( const char *name, mode_t mode, dev_t dev );
PRIVATE int mfatic_mkdir ( const char *name, mode_t mode );
PRIVATE int mfatic_unlink ( const char *name );
PRIVATE int mfatic_rmdir ( const char *name );

// read the contents of a directory.
PRIVATE int mfatic_opendir ( const char *path, struct fuse_file_info *fd );
PRIVATE int mfatic_readdir ( const char *path, void *buf, fuse_fill_dir_t
  *filler, off_t offset, struct fuse_file_info *fd );
PRIVATE int mfatic_releasedir ( const char *path, struct fuse_file_info *fd );

// rename a file
PRIVATE int mfatic_rename ( const char *old, const char *new );

// truncate a file to length bytes.
PRIVATE int mfatic_truncate ( const char *path, off_t length );

// change the access and/or modification times of a file.
PRIVATE int mfatic_utime ( const char *path, const struct utimbuf *time );

// sync a file, and flush cached data on a close.
PRIVATE int mfatic_fsync ( const char *path, int datasync,
  struct fuse_file_info *fd );
PRIVATE int mfatic_flush ( const char *path, struct fuse_file_info *fd );


// Pointer to a struct containing information about the mounted file system
PRIVATE fat_volume_t *v;


// This struct is used by the main loop in the FUSE library to dispatch
// to our methods for handling operations on an mfatic fs, such as open,
// read write and so on.
PRIVATE struct fuse_operations mfatic_ops =
{
    .getattr	= mfatic_getattr,
    .access	= mfatic_access,
    .opendir	= mfatic_opendir,
    .readdir	= mfatic_readdir,
    .releasedir	= mfatic_releasedir,
    .mknod	= mfatic_mknod,
    .mkdir	= mfatic_mkdir,
    .unlink	= mfatic_unlink,
    .rmdir	= mfatic_rmdir,
    .rename	= mfatic_rename,
    .truncate	= mfatic_truncate,
    .utime	= mfatic_utime,
    .open	= mfatic_open,
    .read	= mfatic_read,
    .write	= mfatic_write,
    .release	= mfatic_release,
    .statfs	= mfatic_statfs,
    .fsync	= mfatic_fsync,
    .flush	= mfatic_flush
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
mfatic_open ( path, fd )
    const char *path;		// path from mount point to the file.
    struct fuse_file_info *fd;
{
    // allocate memory for the file handle.
    fat_file_t *newfile = safe_malloc ( sizeof ( fat_file_t ) );
    int retval;

    // fill in the volume and mode fields for fat_open.
    newfile->v = v;
    newfile->mode = fd->flags;

    // open the file. If it fails, return an error, and release the now
    // unneeded file struct back to the pool of free memory.
    if ( ( retval = fat_open ( path, newfile ) ) != 0 )
    {
	safe_free ( &newfile );
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
mfatic_release ( path, fd )
    const char *path;		// absolute path. Unused.
    struct fuse_file_info *fd;
{
    fat_file_t *oldfd = ( fat_file_t * ) fd->fh;

    // release the memory allocated to the file struct.
    fat_close ( oldfd );
    
    // release the file struct, and clear the reference in fd.
    safe_free ( &oldfd );

    return 0;
}

/**
 *  Read nbytes from a file, starting at offset bytes from the start, and
 *  store them in buf.
 *
 *  Return value is the number of bytes read, or EOF.
 */
    PRIVATE int
mfatic_read ( name, buf, nbytes, offset, fd )
    const char *name;		// file name. Unused.
    char *buf;			// buffer to store data read.
    int nbytes;			// no of bytes to read.
    off_t offset;		// where to start reading.
    struct fuse_file_info *fd;	// file handle.
{
    fat_file_t *rf = ( fat_file_t * ) fd->fh;

    // seek to the start offset requested.
    if ( fat_seek ( rf, offset, SEEK_SET ) != offset )
	return EOF;

    // read the data.
    return fat_read ( rf, buf, nbytes );
}
