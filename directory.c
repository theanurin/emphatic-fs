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


/**
 *  locate the directory entry corresponding to a given file. If successful
 *  (ie. the directory entry is found) then this function will return 0,
 *  and the directory entry struct pointed to by the third parameter will
 *  be filled in. If the lookup fails, the return value will be a negative
 *  errno.
 */
    PUBLIC int
fat_lookup_dir ( v, path, d )
    fat_volume_t *v;    // information about the volume.
    const char *path;   // file name to look up.
    fat_direntry_t *d;  // pointer to the direntry struct to be filled in.
{
    char *file;
    fat_direntry_t parent_entry;

    // base case of the recursion is when the path is empty. This ocurrs
    // when we are looking up the root directory.
    if ( *path == '\0' )
    {
        // fill in the directory structure with dummy values, as the root
        // directory does not *actually* ocurr in any directory.
        return root_direntry ( v, d );
    }

    // step backwards through the path until we find the '/' separator.
    // We will replace it, temporarily, with '\0'; that way the parent dir
    // is the string on the left (now terminated where the separator was)
    // and the file we have to look up is the string to the right of the
    // null byte.
    for ( file = path + strlen ( path ); *file != '/'; file -= 1 )
        ;

    *file = '\0';
    file += 1;

    // open the parent directory.
    if ( ( retval = fat_open ( path, &parent_fd ) ) != 0 )
        return retval;

    // check the parent file descriptor's attribute bits, to make sure
    // it is a directory, not a mangled path.
    if ( ( retval = check_directory_attr ( &parent_fd ) ) != 0 )
    {
        // bad path. close the file descriptor and return an error.
        fat_close ( parent_fd );
        return retval;
    }

    // search the parent directory for our file's entry.
    if ( ( retval = dir_search ( parent_fd, file, d ) ) != 0 )
    {
        fat_close ( parent_fd );
        return retval;
    }

    // now restore the path we were given as a parameter back to it's
    // original state.
    ( -- file ) = '/';

    return 0;
}


// vim: ts=4 sw=4 et
