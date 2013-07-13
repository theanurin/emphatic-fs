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

// date and time stamps are stored in 16 bit values.
typedef uint16_t dos_time_t;
typedef uint16_t dos_date_t;


// translate DOS date and time to UNIX time_t.
extern time_t unix_time (dos_date_t dos_date, dos_time_t dos_time);

// translate UNIX time_t to a DOS date and time pair.
extern dos_date_t dos_date (time_t utime);
extern dos_time_t dos_time (time_t utime);

// update the accessed and modified time stamps on a file.
extern void update_atime (const fat_file_t *fd, time_t new_atime);
extern void update_mtime (const fat_file_t *fd, time_t new_mtime);


#endif // MFATIC_DOSTIMES_H

// vim: ts=4 sw=4 et
