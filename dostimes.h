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

// update the accessed and modified time stamps on a file.
extern void update_atime (const fat_file_t *fd, time_t new_atime);
extern void update_mtime (const fat_file_t *fd, time_t new_mtime);


#endif // MFATIC_DOSTIMES_H

// vim: ts=4 sw=4 et
