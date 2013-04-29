/**
 *  utils.h
 *
 *  Wrappers for common functions, which catch errors.
 *
 *  Author: Matthew Signorini
 */

#ifndef UTILS_H
#define UTILS_H

extern void * safe_malloc ( size_t nbytes );
extern void safe_free ( void **freepp );

#endif // UTILS_H
