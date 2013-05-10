/**
 *  utils.c
 *
 *  Implementations for procedures defined in utils.h. Provides 'safe'
 *  wrappers to functions like malloc, checking for any errors that may
 *  occurr.
 *
 *  Author: Matthew Signorini
 */

#include <stdlib.h>
#include <assert.h>
#include <err.h>

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
