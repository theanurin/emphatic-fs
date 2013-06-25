/**
 *  dostimes.h
 *
 *  Procedures for translating time data from the DOS format used in the
 *  FAT file system, to UNIX time, and vice versa.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_DOSTIMES_H
#define MFATIC_DOSTIMES_H


// translate DOS date and time to UNIX time_t.
extern time_t unix_time (unsigned int dos_date, unsigned int dos_time);

// translate UNIX time_t to a DOS date and time pair.
extern unsigned int dos_date (time_t utime);
extern unsigned int dos_time (time_t utime);


#endif // MFATIC_DOSTIMES_H

// vim: ts=4 sw=4 et
