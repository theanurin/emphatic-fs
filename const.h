/**
 *  const.h
 *
 *  Miscelaneous constants used by the Emphatic file system tools.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_CONST_H
#define MFATIC_CONST_H


// allow the scope of a variable or procedure to be restricted to the file
// in which it is defined.
#define PRIVATE         static
#define PUBLIC

// definitions for a boolean type.
typedef int bool;

#define true            (1 != 0)
#define false           (0 != 0)


#endif // MFATIC_CONST_H

// vim: ts=4 sw=4 et
