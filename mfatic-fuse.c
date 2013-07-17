/**
 *  mfatic-fuse.c
 *
 *  Fuse daemon for mounting an Emphatic file system, and methods for
 *  carrying out file operations on an Emphatic fs.
 *
 *  Author: Matthew Signorini
 */

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fuse.h>

#include "mfatic-config.h"
#include "const.h"
#include "utils.h"
#include "fat.h"
#include "directory.h"
#include "fat_alloc.h"
#include "stat.h"
#include "table.h"
#include "create.h"
#include "dostimes.h"
#include "fileio.h"


// minimum number of paramaters for mounting a volume, and offsets of
// device and directory from the *end* of argv.
#define MIN_ARGS                2
#define DEVICE_INDEX            2
#define MOUNTPOINT_INDEX        1

// array indices for the two item array passed to the utimens method.
#define ATIME_INDEX             0
#define MTIME_INDEX             1


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
PRIVATE int mfatic_readdir (const char *path, void *buf, 
  fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fd);
PRIVATE int mfatic_rename (const char *old, const char *new);
PRIVATE int mfatic_truncate (const char *path, off_t length);
PRIVATE int mfatic_utimens (const char *path, const struct timespec *tv);

// functions used by the main program of the FUSE daemon.
PRIVATE void parse_command_opts (int argc, char **argv);
PRIVATE void init_volume (const char *devname, fat_volume_t **volinfo);
PRIVATE bool verify_magic (const char *str1, const char *str2, 
  unsigned int length);
PRIVATE void print_usage (void);
PRIVATE void print_version (void);


// Pointer to a struct containing information about the mounted file system
PRIVATE fat_volume_t *volume_info;

// This struct is used by the main loop in the FUSE library to dispatch
// to our methods for handling operations on an mfatic fs, such as open,
// read write and so on.
PRIVATE struct fuse_operations mfatic_callbacks;

// pointer to a string containing the name of the device file that 
// contains the file system being mounted.
PRIVATE char *device_file;


/**
 *  Program to mount a FAT32 file system using the FUSE framework.
 */
    PUBLIC int
main (argc, argv)
    int argc;       // number of command line parameters.
    char **argv;    // list of parameters.
{
    int retval;

    // initialise the FUSE callbacks structure.
    mfatic_callbacks.getattr    = mfatic_getattr;
    mfatic_callbacks.mknod      = mfatic_mknod;
    mfatic_callbacks.mkdir      = mfatic_mkdir;
    mfatic_callbacks.unlink     = mfatic_unlink;
    mfatic_callbacks.rmdir      = mfatic_unlink;
    mfatic_callbacks.rename     = mfatic_rename;
    mfatic_callbacks.truncate   = mfatic_truncate;
    mfatic_callbacks.open       = mfatic_open;
    mfatic_callbacks.read       = mfatic_read;
    mfatic_callbacks.write      = mfatic_write;
    mfatic_callbacks.statfs     = mfatic_statfs;
    mfatic_callbacks.release    = mfatic_release;
    mfatic_callbacks.opendir    = mfatic_open;
    mfatic_callbacks.readdir    = mfatic_readdir;
    mfatic_callbacks.releasedir = mfatic_release;
    mfatic_callbacks.init       = mfatic_mount;
    mfatic_callbacks.utimens    = mfatic_utimens;

    // process any options salient to the FUSE daemon. This is only
    // really help and version; other options are passed on to the FUSE
    // framework.
    parse_command_opts (argc, argv);

    // attempt to open the device file, and read the super block and other
    // important structures.
    init_volume (device_file, &volume_info);

    // enter the FUSE framework. This will result in the program becoming
    // a daemon.
    retval = fuse_main (argc - 1, argv, &mfatic_callbacks, NULL);

    return retval;
}

/**
 *  Complete the mounting process by invoking the init procedures of the
 *  various components of the Emphatic FUSE daemon. This procedure involves
 *  some IO heavy stuff, like scanning through the entire FAT in order to
 *  map out where the free space is on the device, so it is a Good Thing
 *  that it is done after the mount program has daemonised.
 *
 *  The return value from this struct is kept by FUSE in the private_data
 *  field of fuse_context. Seeing as we do not make use of that, this
 *  procedure will return NULL.
 */
    PRIVATE void *
mfatic_mount (conn)
    struct fuse_conn_info *conn;    // ignored.
{
    // call all the init functions.
    directory_init (volume_info);
    init_clusters_map (volume_info);
    fileio_init (volume_info);
    stat_init (volume_info);
    table_init (volume_info);

    return NULL;
}

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
    fd->fh = (int) newfile;

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
    size_t nbytes;              // no of bytes to read.
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
    size_t nbytes;              // length of the buffer.
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
mfatic_getattr (path, st)
    const char *path;       // file to get information on.
    struct stat *st;        // buffer to store the information.
{
    fat_direntry_t file_info;
    fat_file_t *parent_fd;
    unsigned int index;
    int retval;

    // fetch the directory entry for the file.
    if ((retval = fat_lookup_dir (path, &file_info, &parent_fd, &index)) 
      != 0)
    {
        return retval;
    }

    // we don't need to use the parent directory for anything further.
    fat_close (parent_fd);

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
    fuse_fill_dir_t filler;         // function to fill the buffer with.
    off_t offset;                   // index of first direntry to read.
    struct fuse_file_info *fd;      // directory file handle.
{
    fat_file_t *dirfd = (fat_file_t *) fd->fh;
    fat_direntry_t entry;
    struct stat attrs;

    // move the read offset to the start of the first entry to read.
    if (fat_seek (dirfd, offset * sizeof (fat_direntry_t), SEEK_SET) != 
      offset * sizeof (fat_direntry_t))
    {
        return EOF;
    }

    // iteratively read entries until the filler function indicates that
    // we have filled the buffer.
    do
    {
        // read the next entry.
        fat_read (dirfd, &entry, sizeof (fat_direntry_t));

        // unpack file attribute information from the directory entry.
        unpack_attributes (&entry, &attrs);
    }
    while (filler (buffer, entry.fname, &attrs, offset += 1) != 1);

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
    cluster_list_t *prev = NULL;
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
        for (cluster_list_t *cp = fd->current_cluster->next; cp != NULL;
          cp = cp->next)
        {
            release_cluster (cp->cluster_id);

            // release the previous item in the list, if there was a
            // previous.
            if (prev != NULL)
                safe_free ((void **) &prev);

            prev = cp;
        }

        // release the last item in the linked list.
        safe_free ((void **) &prev);
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
    int index, c;
    static const struct option opts [] =
    {
        {"help",    no_argument,    0, 'h'},
        {"version", no_argument,    0, 'v'},
        {0,         0,              0, 0}
    };

    // check for help or version options. getopt_long returns -1 once
    // we run out of command line parameters to process.
    while ((c = getopt_long (argc, argv, "hv", opts, &index)) != -1)
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
    device_file = argv [argc - DEVICE_INDEX];

    // swap the mountpoint and device args for FUSE.
    argv [argc - DEVICE_INDEX] = argv [argc - MOUNTPOINT_INDEX];
}

/**
 *  Open a given device file and attempt to read FAT32 file system data
 *  structures. This procedure will also do some validation (ie. check
 *  magics).
 */
    PRIVATE void
init_volume (devname, volinfo)
    const char *devname;        // device file hosting our file system.
    fat_volume_t **volinfo;     // this will be set by init_volume.
{
    int devfd;
    fat_super_block_t *sb;
    fat_fsinfo_t *fsinfo;

    // open the device file. This will abort on errors.
    devfd = safe_open (devname, O_RDWR);

    // now allocate memory for the various data structures.
    *volinfo = safe_malloc (sizeof (fat_volume_t));
    sb = safe_malloc (sizeof (fat_super_block_t));
    fsinfo = safe_malloc (sizeof (fat_fsinfo_t));

    // read in the FAT32 super block (or BPB, if you are Old School).
    safe_seek (devfd, 0, SEEK_SET);
    safe_read (devfd, sb, sizeof (fat_super_block_t));

    // read in the fs info sector, field by field as it is not a one to
    // one mapping of the on disk structure (we ommit all the unused space
    // to save memory).
    safe_seek (devfd, sb->fsinfo_sector * sb->bps, SEEK_SET);
    safe_read (devfd, &(fsinfo->magic1), FSINFO_MAGIC1_LEN);
    safe_seek (devfd, 480, SEEK_CUR);
    safe_read (devfd, &(fsinfo->magic2), FSINFO_MAGIC2_LEN + 8);
    safe_seek (devfd, 12, SEEK_CUR);
    safe_read (devfd, &(fsinfo->magic3), FSINFO_MAGIC3_LEN);

    // check fsinfo magics.
    if ((verify_magic (FSINFO_MAGIC1, fsinfo->magic1, FSINFO_MAGIC1_LEN) &&
          verify_magic (FSINFO_MAGIC2, fsinfo->magic2, FSINFO_MAGIC2_LEN) &&
          verify_magic (FSINFO_MAGIC3, fsinfo->magic3, FSINFO_MAGIC3_LEN))
      != true)
    {
        // magics don't match. That would indicate that the device is not
        // formatted as a FAT file system, and we should not continue any
        // further with the mounting process.
        fprintf (stderr, "%s : Error: Could not mount %s due to "
          "bad magic.\n Are you sure it is a valid FAT32 file system?",
          PROGNAME, devname);
        exit (1);
    }

    // fill in the volume info structure.
    (*volinfo)->dev_fd = devfd;
    (*volinfo)->bpb = sb;
    (*volinfo)->fsinfo = fsinfo;
}

/**
 *  compare two magics, of a given length, regardless of the presence
 *  of NULL bytes. This procedure steps along the two strings for as long
 *  as they remain identical (including identical null bytes) returning
 *  only when it reaches the specified length to compare, or a non matching
 *  character is found.
 *
 *  Return value is true if the strings are identical, or fals if they
 *  differ.
 */
    PRIVATE bool
verify_magic (str1, str2, length)
    const char *str1;       // expected value.
    const char *str2;       // actual magic on the device.
    unsigned int length;    // length to compare for.
{
    // step along both strings until we either find a differing character,
    // or we reach the end, as specified by length.
    for (unsigned int i = 0; i < length; i ++)
    {
        if (str1 [i] != str2 [i])
            return false;
    }

    // if we reach this point, the strings must be identical.
    return true;
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
      "Version: %s\n\n"
      "This is free and open source software. Please see the file COPYING\n"
      "for the terms under which you may use, modify and redistribute\n"
      "this software. This software has no warranty.\n\n"
      "%s\n", VERSION_STR, COPYRIGHT_STR);
}


// vim: ts=4 sw=4 et
