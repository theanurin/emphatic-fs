/**
 *  Export logging functions for testing the Emphatic FUSE daemon.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_DEBUG_H
#define MFATIC_DEBUG_H


// functions for writing debug messages.
extern void debug_init (const char *file_name);
extern void debug_print (const char *message);


#endif // MFATIC_DEBUG_H

// vim: ts=4 sw=4 et
