/**
 *  utils.h
 *
 *  Wrappers for common functions, which catch errors.
 *
 *  Author: Matthew Signorini
 */

#ifndef UTILS_H
#define UTILS_H


// wrappers for file routines.
extern int safe_open ( const char *path, int flags );
extern void safe_close ( const char *path, int fd );
extern off_t safe_seek ( int fd, off_t offset, int whence );
extern size_t safe_read ( int fd, void *buffer, size_t count );
extern size_t safe_write ( int fd, const void *buffer, size_t count );

// catch null pointers returned by malloc.
extern void * safe_malloc ( size_t nbytes );
extern void safe_free ( void **freepp );


#endif // UTILS_H

// vim: ts=4 sw=4 et
