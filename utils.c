/**
 *  utils.c
 *
 *  Implementations for procedures defined in utils.h. Provides 'safe'
 *  wrappers to functions like malloc, checking for any errors that may
 *  occurr.
 *
 *  Author: Matthew Signorini
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include <stdlib.h>
#include <assert.h>
#include <err.h>

#include "const.h"
#include "utils.h"


/**
 *  wrapper to open system call. Only returns a valid file descriptor.
 */
    PUBLIC int
safe_open ( path, flags )
    const char *path;	// file path for syscall.
    int flags;		// mode, etc.
{
    int fd;

    if ( ( fd = open ( path, flags ) ) == -1 )
	err ( errno, "Couldn't open %s", path );

    return fd;
}

/**
 *  wrapper for the close system call. Aborts on errors.
 */
    PUBLIC void
safe_close ( path, fd )
    const char *path;	// displayed in an error message on failure.
    int fd;		// file descriptor to close.
{
    if ( close ( fd ) != 0 )
	err ( errno, "Error closing file \"%s\"", path );
}

/**
 *  The next two procedures are wrappers for read and write, much the same
 *  as above.
 */
    PUBLIC size_t
safe_read ( fd, buffer, count )
    int fd;		// file descriptor to read from.
    void *buffer;	// buffer to store data read.
    size_t count;	// number of bytes to be read.
{
    size_t nread;

    if ( ( nread = read ( fd, buffer, count ) ) == -1 )
	err ( errno, "Error during read system call" );

    return nread;
}

    PUBLIC size_t
safe_write ( fd, buffer, count )
    int fd;		// file descriptor to write to.
    const void *buffer;	// data to write to the file.
    size_t count;	// number of bytes to write.
{
    size_t nwritten;

    if ( ( nwritten = write ( fd, buffer, count ) ) == -1 )
	err ( errno, "Error during write system call" );

    return nwritten;
}

/**
 *  wrapper to the lseek system call. Return value is the new offset into
 *  the file. This procedure aborts on failure.
 */
    PUBLIC off_t
safe_seek ( fd, offset, whence )
    int fd;		// file to seek on.
    off_t offset;	// offset by which to move the current position.
    int whence;		// where to seek relative to.
{
    off_t new_off;

    if ( ( new_off = lseek ( fd, offset, whence ) ) == -1 )
	err ( errno, "Error during lseek system call" );

    return new_off;
}

/**
 *  wrapper for malloc. Catches return value of NULL.
 */
    PUBLIC void *
safe_malloc ( nbytes )
    size_t nbytes;	// number of bytes to allocate.
{
    void *mem = malloc ( nbytes );

    // TODO: call a program defined abort procedure?
    if ( mem == NULL )
	err ( errno, "Failed allocating memory" );

    return mem;
}

/**
 *  safe alternative to free. This function takes a pointer to the pointer
 *  to the memory region being freed, and clears the pointer to the buffer.
 *  This prevents the program from attempting to access memory after it
 *  has been freed, and allows us to catch an attempt to free an already
 *  free region.
 */
    PUBLIC void
safe_free ( freepp )
    void **freepp;	// address of pointer to the buffer to be freed.
{
    assert ( freepp != NULL );

    // has this buffer already been freed?
    if ( *freepp != NULL )
    {
	// no. free it.
	free ( *freepp );
	*freepp = NULL;
	return;
    }

    // TODO: log an error, or abort?
}
