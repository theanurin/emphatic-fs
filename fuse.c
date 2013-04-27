/**
 *  fuse.c
 *
 *  Fuse methods for the mfatic file system.
 *
 *  Author: Matthew Signorini
 */

#include <fuse.h>


#include "mfatic.h"


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


// This struct is used by the main loop in the FUSE library to dispatch
// to our methods for handling operations on an mfatic fs, such as open,
// read write and so on.
static struct fuse_operations mfatic_ops =
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
