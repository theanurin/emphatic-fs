/**
 *  create.c
 *
 *  Procedures for creating and deleting files on a FAT32 file system.
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "utils.h"
#include "fat.h"
#include "create.h"


// local functions.
PRIVATE char * decompose_path (char *path);
PRIVATE int fat_rmdir (fat_file_t *dirfd);
PRIVATE bool is_reserved_name (const char *name);


// list of reserved file names.
PRIVATE char *reserved_names [] = {".", "..", ""};

#define NR_RESERVED_NAMES           3


/**
 *  Create a new file node, and allocate an initial cluster to it.
 *  This procedure can be used to create either ordinary files or
 *  directories, simply by setting the appropriate attribute bits.
 */
    PUBLIC int
fat_create (path, attributes)
    const char *path;       // path, including file name to create.
    fat_attr_t attributes;  // attributes for the new file.
{
    char *parent = strdupa (path), *file;
    fat_entry_t new_node;
    fat_direntry_t new_entry;
    time_t now = time (NULL);
    dos_time_t time_now = dos_time (now);
    dos_date_t date_now = dos_date (now);

    // break up the path name into a parent directory, and a file to be
    // created.
    file = decompose_path (parent);

    // attempt to open the parent dir. Return an error code if this fails.
    if ((retval = fat_open (parent, &parent_fd)) != 0)
        return retval;

    // build the directory entry for the new file.
    new_node = fat_alloc_node ();
    strncpy (new_entry.fname, file, DIR_NAME_LEN);
    new_entry.name [DIR_NAME_LEN - 1] = '\0';
    new_entry.attributes = attributes;
    new_entry.creation_tenths = 0;
    new_entry.creation_time = time_now;
    new_entry.creation_date = date_now;
    new_entry.access_date = date_now;
    new_entry.write_time = time_now;
    new_entry.write_date = date_now;
    PUT_DIRENTRY_CLUSTER (&new_entry, new_node);
    new_entry.size = 0;

    // write in the new directory entry.
    dir_write_entry (parent_fd, &new_entry);
    fat_close (parent_fd);

    return 0;
}

/**
 *  Rename an existing file. The oldpath param must be the name of an
 *  existing file, and the newpath param must not point to an existing
 *  file, or refer to nonexistent directories in the path. An error code 
 *  will be returned if these conditions are not met.
 *
 *  Return value is 0 on success, or a negative errno on failure.
 */
    PUBLIC int
fat_rename (oldpath, newpath)
    const char *oldpath;    // existing file to rename.
    const char *newpath;    // must not be an existing file.
{
    char *newparent = strdupa (newpath), *newfile;

    // break the paths into a path to a parent dir, and a file name.
    newfile = decompose_path (newparent);

    // open the destination directory.
    if ((retval = fat_open (newparent, &newfd)) != 0)
        return retval;

    // look up the file to rename in the source directory.
    if ((retval = fat_lookup_dir (oldpath, &entry, &oldfd, &index)) != 0)
    {
        // some error looking up the file. Abort.
        fat_close (newfd);

        // check if the parent directory was opened.
        if (oldfd != NULL)
            fat_close (oldfd);

        return retval;
    }

    // delete the entry from the source directory.
    dir_delete_entry (oldfd, index);

    // apply the new name.
    strncpy (entry.fname, newfile, DIR_NAME_LEN);
    entry.fname [DIR_NAME_LEN - 1] = '\0';

    // write the entry to the destination of the move operation.
    dir_write_entry (newfd, &entry);

    // finished.
    fat_close (oldfd);
    fat_close (newfd);

    return 0;
}

/**
 *  Mark a file for deletion. The file will only actually be deleted once
 *  all open references to it have been closed, so this routine just sets
 *  a bit in a flags bitmap. The file must be writable, and must not be a
 *  directory.
 *
 *  Return value is 0 on success or a negative errno on failure.
 */
    PUBLIC int
fat_unlink (filename)
    const char *filename;   // absolute path to the file to delete.
{
    fat_file_t *fd;

    // open the file. If there are other open instances, this just creates
    // another reference in the open file table.
    if ((retval = fat_open (filename, &fd)) != 0)
        return retval;

    // check the attribute bits.
    if ((fd->attributes & ATTR_READ_ONLY) != 0)
    {
        // cannot delete a read only file.
        fat_close (fd);
        return -EACCES;
    }

    // if it is a directory, we need to verify that the directory is empty.
    if ((fd->attributes & ATTR_DIRECTORY) != 0)
        return fat_rmdir (fd);

    // set the flags bit to indicate that this file is to be released once
    // all open references have been closed.
    fd->flags |= FL_DELETE_ON_CLOSE;

    // finished. If we have the only open reference to the file, it will
    // be deleted now, when we close it.
    fat_close (fd);

    return 0;
}

/**
 *  Release the resources allocated to a file on deletion. This procedure
 *  will remove the file's directory entry, and release all the clusters
 *  allocated to it back to the free pool.
 */
    PUBLIC void
fat_release (fd)
    fat_file_t *fd;     // file that is being deleted.
{
    cluster_list_t *cp;

    // delete the file's directory entry.
    dir_delete_entry (get_parent_fd (fd->directory_inode), 
      fd->dir_entry_index);

    // step through the list of clusters, and release each one. We will
    // not actually free the memory here; the caller has to do that.
    for (cp = fd->clusters; cp != NULL; cp = cp->next)
    {
        release_cluster (cp->cluster_id);
    }
}

/**
 *  Mark a directory for deletion. This will test that the directory is
 *  empty, but assumes that the caller has confirmed that it is not read
 *  only.
 *
 *  Return value is 0 on success, or a negative errno if the directory is
 *  not empty.
 */
    PRIVATE int
fat_rmdir (dirfd)
    fat_file_t *dirfd;  // directory file to be marked for deletion.
{
    fat_direntry_t buffer;

    // The directory file may have non zero size, as it could have a few
    // blank entries (identified because their name starts with a null
    // byte). Check any entries that are present to make sure they are
    // not important.
    while (fat_read (dirfd, &buffer, sizeof (fat_direntry_t)) != 0)
    {
        if (is_reserved_name (buffer->fname) != true)
        {
            // directory is not empty. Cannot be deleted.
            fat_close (dirfd);
            return -ENOTEMPTY;
        }
    }

    // mark the directory for deletion.
    dirfd->flags |= FL_DELETE_ON_CLOSE;
    fat_close (dirfd);

    return 0;
}

/**
 *  Break up a path name into a parent directory and a file. This works by
 *  replacing a single path name separator, between the parent directory
 *  and the file, with a null byte. As such, the parent directory is the
 *  string pointed to by the parameter. Return value is a pointer to a
 *  string containing the file name.
 */
    PRIVATE char *
decompose_path (pathname)
    char *pathname;     // original path name to break up.
{
    char *file = pathname + strlen (pathname);

    // step the file pointer back until we find a path name separator.
    for ( ; *file != '/'; file -= 1)
        ;

    // replace the separator with a null byte, and step the file pointer
    // to the following character.
    *file = '\0';
    file += 1;

    return file;
}

/**
 *  Check if a name matches with a reserved name, as defined in the global
 *  list.
 */
    PRIVATE bool
is_reserved_name (name)
    const char *name;   // name to test.
{
    // step through the reserved names one by one, and return true if we
    // find a match.
    for (int i = 0; i < NR_RESERVED_NAMES; i ++)
    {
        if (strcmp (reserved_names [i], name) == 0)
            return true;
    }

    // name was not found in the list of reserved names.
    return false;
}


// vim: ts=4 sw=4 et
