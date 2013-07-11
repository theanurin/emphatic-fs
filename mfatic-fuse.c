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
PRIVATE void * mfatic_mount (struct fuse_conn_info *conn);
PRIVATE int mfatic_open (const char *name, struct fuse_file_info *fd);
PRIVATE int mfatic_read (const char *name, char *buf, size_t nbytes, 
  off_t off, struct fuse_file_info *fd);
PRIVATE int mfatic_write (const char *name, const char *buf, size_t nbytes,
  off_t off, struct fuse_file_info *fd);
PRIVATE int mfatic_release (const char *name, struct fuse_file_info *fd);
PRIVATE int mfatic_getattr (const char *name, struct stat *st);
PRIVATE int mfatic_statfs (const char *name, struct statvfs *st);
PRIVATE int mfatic_mknod (const char *name, mode_t mode, dev_t dev);
PRIVATE int mfatic_mkdir (const char *name, mode_t mode);
PRIVATE int mfatic_unlink (const char *name);
PRIVATE int mfatic_readdir (const char *path, void *buf, fuse_fill_dir_t
  *filler, off_t offset, struct fuse_file_info *fd);
PRIVATE int mfatic_rename (const char *old, const char *new);
PRIVATE int mfatic_truncate (const char *path, off_t length);
PRIVATE int mfatic_utimens (const char *path, const struct timespec *tv);

// functions used by the main program of the FUSE daemon.
PRIVATE int parse_command_opts (int argc, char **argv);
PRIVATE void print_usage (void);
PRIVATE void print_version (void);


// Pointer to a struct containing information about the mounted file system
PRIVATE fat_volume_t *volume_info;


// This struct is used by the main loop in the FUSE library to dispatch
// to our methods for handling operations on an mfatic fs, such as open,
// read write and so on.
PRIVATE struct fuse_operations mfatic_ops =
{
    .init       = mfatic_mount,
    .getattr    = mfatic_getattr,
    .opendir    = mfatic_open,
    .readdir    = mfatic_readdir,
    .releasedir = mfatic_release,
    .mknod      = mfatic_mknod,
    .mkdir      = mfatic_mkdir,
    .unlink     = mfatic_unlink,
    .rmdir      = mfatic_unlink,
    .rename     = mfatic_rename,
    .truncate   = mfatic_truncate,
    .utimens    = mfatic_utimens,
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

    // update the access time field for this file.
    update_atime (rf, time (NULL));

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

    // update the time of last modification.
    update_mtime (wf, time (NULL));

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
 *  Create an ordinary file.
 */
    PRIVATE int
mfatic_mknod (name, mode, dev)
    const char *name;       // name of file to create.
    mode_t mode;            // ignored, at present.
    dev_t dev;              // not used.
{
    fat_attr_t new_attributes = 0;

    return fat_create (name, new_attributes);
}

/**
 *  Create a directory.
 */
    PRIVATE int
mfatic_mkdir (name, mode)
    const char *name;       // path to new directory.
    mode_t mode;            // ignored, at present.
{
    fat_attr_t dir_attrs = ATTR_DIRECTORY;

    return fat_create (name, dir_attrs);
}

/**
 *  Remove a file system node. This procedure handles both ordinary files
 *  and directories, by assuming that it is an rmdir operation if invoked
 *  on a directory.
 */
    PRIVATE int
mfatic_unlink (name)
    const char *name;   // absolute path of the node to be removed.
{
    return fat_unlink (name);
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

/**
 *  Change a file's name, and potentially parent directory.
 */
    PRIVATE int
mfatic_rename (old, new)
    const char *old;    // file to be renamed.
    const char *new;    // new name.
{
    return fat_rename (old, new);
}

/**
 *  Truncate a given file to a given length. If the file is originally
 *  longer than the specified length, the extra data is lost; if the file
 *  is shorter, extra clusters will be allocated, and zeroed.
 *
 *  Return value is 0 on success, or a negative errno on failure.
 */
    PRIVATE int
mfatic_truncate (path, length)
    const char *path;       // target file.
    off_t length;           // length to truncate to.
{
    fat_file_t *fd;
    int retval;
    const char zero = '\0';
    fat_cluster_t *prev = NULL;
    size_t oldsize;

    // open the target file.
    if ((retval = fat_open (path, &fd)) != 0)
        return retval;

    // change the file size, and seek to EOF.
    oldsize = fd->size;
    fd->size = (size_t) length;
    fat_seek (fd, 0, SEEK_END);

    // check whether the size of the file is greater than or less than the
    // length we are truncating it to.
    if (oldsize > length)
    {
        // truncated legnth is shorter than existing size, so we will
        // delete the excess, releasing clusters to the free pool.
        // First, mark the current cluster as End of File.
        put_fat_entry (fd->current_cluster->cluster_id, END_CLUSTER_MARK);

        // now step through all the remaining clusters in the list, and
        // release them to the free space pool.
        for (fat_cluster_t *cp = fd->current_cluster->next; cp != NULL;
          cp = cp->next)
        {
            release_cluster (cp->cluster_id);

            // release the previous item in the list, if there was a
            // previous.
            if (prev != NULL)
                safe_free (&prev);

            prev = cp;
        }

        // release the last item in the linked list.
        safe_free (&prev);
    }
    else if (oldsize < length)
    {
        // truncatesd length is longer than the existing file size. In
        // this case, we allocate extra clusters, and zero them. This can
        // be done simply with a write operation.
        for (int i = 0; i < (length - oldsize); i ++)
        {
            fat_write (fd, &zero, 1);
        }
    }

    return 0;
}

/**
 *  Change the access and/or modification times for a given file to given
 *  values.
 */
    PRIVATE int
mfatic_utimens (path, tv)
    const char *path;               // path to the target file.
    const struct timespec *tv;      // array containing the times to set.
{
    fat_file_t *fd;
    int retval;

    // open the target file.
    if ((retval = fat_open (path, &fd)) != 0)
        return retval;

    // The user must have write permission on the file in order to modify
    // it's times. Check that the file is not read only.
    if ((fd->attributes & ATTR_READ_ONLY) != 0)
    {
        // cannot change time stamps on a read only file.
        fat_close (fd);
        return -EACCES;
    }

    // write in the new time values.
    update_atime (fd, tv [ATIME_INDEX].tv_sec);
    update_mtime (fd, tv [MTIME_INDEX].tv_sec);

    // done.
    fat_close (fd);

    return 0;
}

/**
 *  Parse any command line options given to the Emphatic mount command
 *  line. Note that this procedure only deals with Emphatic specific
 *  options (help and version), and there may also be options for FUSE,
 *  which will be ignored by this procedure.
 */
    PRIVATE void
parse_command_opts (argc, argv)
    int argc;           // number of options given.
    char **argv;        // vector of text options.
{
    int option_index, c;
    static struct options [] =
    {
        {"help",    no_argument,    0, 'h'},
        {"version", no_argument,    0, 'v'},
        {0,         0,              0, 0}
    };

    // check for help or version options. getopt_long returns -1 once
    // we run out of command line parameters to process.
    while ((c = getopt_long (argc, argv, "hv", options, &option_index))
      != -1)
    {
        switch (c)
        {
        case 'h':
            // print usage info and exit.
            print_usage ();
            exit (0);
            break;

        case 'v':
            // print version info and exit.
            print_version ();
            exit (0);
        }
    }

    // If we reach this point, we must be mounting a device. Do a sanity
    // test on the length of the parameter list provided on the command
    // line, to attempt to catch errors, like not specifying a device or
    // mount point.
    if (argc < MIN_ARGS)
    {
        // too few parameters.
        print_usage ();
        exit (0);
    }

    // device and mountpoint parameters should be at the end of the
    // parameter list.
    device_arg = argv [argc - DEVICE_INDEX];

    // swap the mountpoint and device args for FUSE.
    argv [argc - DEVICE_INDEX] = argv [argc - MOUNTPOINT_INDEX];
}

/**
 *  Print out usage information for the Emphatic FUSE daemon.
 */
    PRIVATE void
print_usage (void)
{
    printf ("mfatic-fuse: FUSE mount tool for FAT32 file systems.\n\n"
      "USAGE:\n"
      "\tmfatic-fuse [-hv]\n"
      "\tmfatic-fuse [options] device directory\n\n"
      "COMMAND LINE OPTIONS:\n"
      "\t-h --help    print this information\n"
      "\t-v --version print version information\n"
      "\toptions      FUSE specific options. See the man page for\n"
      "\t             fuse(8) for a list.\n");
}

/**
 *  Print out the version of the Emphatic FUSE daemon being used.
 */
    PRIVATE void
print_version (void)
{
    printf ("mfatic-fuse: FUSE mount tool for FAT32 file systems.\n\n"
      "Version: " VERSION_STR "\n\n"
      "This is free and open source software. Please see the file COPYING\n"
      "for the terms under which you may use, modify and redistribute\n"
      "this software. This software has no warranty.\n\n"
      COPYRIGHT_STR "\n");
}


// vim: ts=4 sw=4 et
