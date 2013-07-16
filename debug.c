/**
 *  debug.c
 *
 *  Procedures for printing out debugging messages.
 *
 *  Author: Matthew Signorini
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "const.h"
#include "debug.h"


// global file handle for the log file.
PRIVATE FILE *logfd;


/**
 *  attempt to open the log file. If the file name is empty, we will
 *  assume that no file is to be opened.
 */
    PUBLIC void
debug_init (file_name)
    const char *file_name;      // log file name.
{
    // check if the name is empty.
    if (*file_name == '\0')
        return;

    // open the log file. If it fails, we will print an error message
    // to stderr.
    if ((logfd = fopen (file_name, "w")) == NULL)
    {
        fprintf (stderr, "mfatic-fuse: fopen: Could not open %s: %s\n",
          file_name, strerror (errno));
        fprintf (stderr, "Log output is disabled.\n");
    }
}

/**
 *  Print a debugging message to the log file.
 */
    PUBLIC void
debug_print (format)
    const char *format;         // printf style format string.
{
    va_list args;

    va_start (args, format);

    // check that we have a log file to write to.
    if (logfd == NULL)
        return;

    vfprintf (logfd, format, args);
    fflush (logfd);

    va_end (args);
}


// vim: ts=4 sw=4 et