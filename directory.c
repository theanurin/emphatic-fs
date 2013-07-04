/**
 *  directory.c
 *
 *  Provides implementations for procedures declared in directory.h.
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "utils.h"
#include "fat.h"
#include "directory.h"


// local functions.
PRIVATE int root_direntry (fat_direntry_t *buffer);
PRIVATE bool is_directory (fat_file_t *file);
PRIVATE bool search_directory (fat_file_t *dir, const char *name,
  fat_direntry_t *found, unsigned int *index);
PRIVATE void remove_separators (char *pathname);


// global pointer to the volume information struct for the mounted file
// system.
PRIVATE fat_volume_t *volume_info;

// list of currently active directories.
PRIVATE file_list_t *active_dirs;


/**
 *  This should be called at mount time, to initialise the volume info
 *  structure.
 */
    PUBLIC void
directory_init (v)
    const fat_volume_info *v;   // info about the mounted file system.
{
    volume_info = v;
    active_dirs = NULL;
}

/**
 *  locate the directory entry corresponding to a given file. If successful
 *  (ie. the directory entry is found) then this function will return 0,
 *  and the directory entry struct pointed to by the third parameter will
 *  be filled in. If the lookup fails, the return value will be a negative
 *  errno.
 */
    PUBLIC int
fat_lookup_dir (path, buffer, inode, index)
    const char *path;           // file name to look up.
    fat_direntry_t *buffer;     // pointer to the direntry struct to fill.
    fat_entry_t *inode;         // put parent dir's i-node number here.
    unsigned int *index;        // put dir index here.
{
    char *file = strdupa (path);
    fat_file_t *parent_fd;

    // first, we will convert all the path separators into null bytes,
    // so that the path becomes a collection of file name strings.
    remove_separators (file);

    // get the root directory entry, to begin the search.
    root_direntry (buffer);

    // Traverse the directory tree. This is done by opening whatever file
    // buffer refers to, and copying file's dir entry into buffer.
    // Then file becomes the new parent, and the next string in the path
    // name becomes the new file.
    do
    {
        fat_open (buffer, 0, 0, &parent_fd);
        *parent_inode = DIR_CLUSTER_START (buffer);

        // check the file is a directory.
        if (is_directory (parent_fd) != true)
        {
            // no. Bad path name.
            fat_close (parent_fd);
            return -ENOTDIR;
        }

        // search the directory for the next file in the path.
        if (search_directory (parent_fd, file, buffer, child_index) !=
          true)
        {
            // no such file or directory.
            fat_close (parent_fd);
            return -ENOENT;
        }

        // move on to the next component of the path name.
        file += strlen (file) + 1;

        // If we have finished path name translation, we want to keep
        // the file's parent directory open, to add it to the list of
        // active directories.
        if (*file != '\0')
            fat_close (parent_fd);
    }
    while (*file != '\0');

    // Add the file's parent directory to the active directories list.
    ilist_add (active_dirs, parent_fd);

    return 0;
}

/**
 *  Read a file's directory entry in O(1) time. This uses the directories
 *  i-node to find it's file descriptor, and an entry index to locate the
 *  desired entry.
 */
    PUBLIC void
get_directory_entry (buffer, inode, index)
    fat_direntry_t *buffer;         // buffer to store the entry.
    fat_entry_t inode;              // identifies the directory to read.
    unsigned int index;             // entry to read.
{
    fat_file_t *dirfd;

    // look up the directories fd in our list of active dirs.
    if (ilist_lookup_file (active_dirs, &dirfd, inode) != true)
        return;

    // seek to the entry requested, and read it into the caller's buffer.
    fat_seek (dirfd, index * sizeof (fat_direntry_t), SEEK_SET);
    fat_read (dirfd, buffer, sizeof (fat_direntry_t));

    // decrement the refcount that was incremented by the lookup
    // operation.
    ilist_unlink (active_dirs, inode);
}

/**
 *  Overwrite an existing directory entry with new values.
 */
    PUBLIC void
put_directory_entry (buffer, inode, index)
    const fat_direntry_t *buffer;   // new entry to write.
    fat_entry_t inode;              // identifies directory to write on.
    unsigned int index;             // entry to overwrite.
{
    fat_file_t *dirfd;

    // lookup the directories fd.
    if (ilist_lookup_file (active_dirs, &dirfd, inode) != true)
        return;

    // seek to the appropriate entry, and write.
    fat_seek (dirfd, index * sizeof (fat_direntry_t), SEEK_SET);
    fat_write (dirfd, buffer, sizeof (fat_direntry_t));

    // correct the refcount in the active dirs list.
    ilist_unlink (active_dirs, inode);
}

/**
 *  Release a file's parent directory from the list of active directories.
 */
    PUBLIC void
release_parent_dir (fd)
    const fat_file_t *fd;       // target file.
{
    ilist_unlink (active_dirs, fd->directory_inode);
}

/**
 *  Fill in a directory entry struct with the appropriate fields for the
 *  root directory of the mounted FAT file system.
 *
 *  This function will always succeed, with a return value of 0.
 */
    PRIVATE int
root_direntry (entry_buf)
    fat_direntry_t *entry_buf;      // structure to fill in.
{
    // name is just '/'.
    entry_buf->fname [0] = '/';

    // set the directory attribute bit.
    entry_buf->attributes = ATTR_DIRECTORY;

    // get the root directory cluster from the volume information struct.
    PUT_DIRENTRY_CLUSTER (entry_buf, volume_info->bpb->root_cluster);

    return 0;
}

/**
 *  Test if a given file is a directory or not.
 *
 *  Return value is true if the file is a directory, false if not.
 */
    PRIVATE bool
is_directory (file)
    fat_file_t *file;   // file descriptor to test.
{
    if ((file->attributes & ATTR_DIRECTORY) != 0)
    {
        // is a directory.
        return true;
    }
    else
    {
        return false;
    }
}

/**
 *  Step through all the entries in a directory until we find one that
 *  matches the name we are looking for.
 *
 *  If a match is found, this function returns true, and the matching entry
 *  is stored at *found. If no match is found, this function returns false.
 */
    PRIVATE bool
search_directory (dir, name, found, index)
    fat_file_t *dir;            // directories file handle for reading.
    const char *name;           // name to look up.
    fat_direntry_t *found;      // buffer to store a match.
    unsigned int *index;        // dir index will be stored here.
{
    // start searching at the start of the directory.
    fat_seek (dir, 0, SEEK_SET);

    // clear the index counter.
    *index = 0;

    // step through all the directory entries in the table.
    do
    {
        // read the next entry. If we have reached the end of the file,
        // ie. read returns 0, then no match was found.
        if (fat_read (dir, &found, sizeof (fat_direntry_t)) == 0)
            return false;

        // increment the index count.
        *index += 1;
    }
    while (strncmp (name, dir->fname, DIR_NAME_LEN) != 0);

    // correct index counter for the final iteration of the loop.
    *index -= 1;

    // we can only exit the loop if a match was found.
    return true;
}

/**
 *  Step through a path name, and turn all the separators into null bytes.
 *  This has the effect of turning a path name into a list of
 *  subdirectories to iteratively search.
 */
    PRIVATE void
remove_separators (pathname)
    char *pathname;     // pointer to pathname to process.
{
    while (*pathname != '\0')
    {
        if (*pathname == '/')
        {
            *pathname = '\0';
        }

        pathname += 1;
    }
}


// vim: ts=4 sw=4 et
