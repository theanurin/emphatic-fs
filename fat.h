/**
 *  fat.h
 *
 *  Constants types and procedures used by the mfatic file system.
 *
 *  Because mfatic is intended to be compatible with the standard FAT
 *  file system, most of the data structures are the same.
 *
 *  Author: Matthew Signorini
 */

#ifndef MFATIC_FAT_H
#define MFATIC_FAT_H


#define OEM_LEN		8
#define LABEL_LEN	11
#define DIR_NAME_LEN	11


// These macros can be used to test if a given entry in the file allocation
// table corresponds to the last cluster in a file, or a cluster marked as
// bad.
#ifdef MFATIC_32
#define IS_LAST_CLUSTER( entry ) ( ( ( entry ) & 0x0FFFFFFF ) >= 0x0FFFFFF8 )
#define IS_BAD_CLUSTER( entry )	 ( ( ( entry ) & 0x0FFFFFFF ) == 0x0FFFFFF7 )
#define IS_FREE_CLUSTER( entry ) ( ( ( entry ) & 0x0FFFFFFF ) == 0x00000000 )

// sentinels for the last cluster or a bad cluster.
#define END_CLUSTER_MARK    0x0FFFFFF8
#define BAD_CLUSTER_MARK    0x0FFFFFF7
#endif // MFATIC_32


// Bit definitions for the attribute bitmap in each directory entry 
// structure.
#define ATTR_READ_ONLY	    0x01
#define ATTR_HIDDEN	    0x02
#define ATTR_SYSTEM	    0x04
#define ATTR_VOLUME_ID	    0x08
#define ATTR_DIRECTORY	    0x10
#define ATTR_ARCHIVE	    0x20


// The following macros are for interpretting the date and time fields
// of directory entries.
//
// Time encoding:
//
//      hi byte     |    low byte
//  |7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
//  | | | | | | | | | | | | | | | | |
//  \  5 bits /\  6 bits  /\ 5 bits /
//      hour      minute     sec*2
#define TIME_HOUR( t )	    ( ( ( t ) & 0xF800 ) >> 11 )
#define TIME_MINUTE( t )    ( ( ( t ) & 0x07E0 ) >> 5 )
#define TIME_SECOND( t )    ( ( ( t ) & 0x001F ) << 1 )

// Date encoding: Note that dates are relative to the DOS epoch of 
// 00:00:00 UTC 1 Jan 1980.
//
//      hi byte     |    low byte
//  |7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
//  | | | | | | | | | | | | | | | | |
//  \   7 bits    /\4 bits/\ 5 bits /
//    year + 1980    month    day
#define DATE_YEAR( d )	    ( ( ( ( d ) & 0xFE00 ) >> 9 ) + 1980 )
#define DATE_MONTH( d )	    ( ( ( d ) & 0x01E0 ) >> 5 )
#define DATE_DAY( d )	    ( ( d ) & 0x001F )


/**
 *  FAT32 Bios Parameter Block.
 *
 *  This structure is analogous to the superblock found on most UNIX type
 *  file systems, and the data contained in this structure is required for
 *  basically all file operations. The on-disk structure is located in the
 *  first sector (the boot sector), and it will be read into an in memory
 *  struct, for obvious efficiency reasons.
 */
typedef struct
{
    uint8_t	jmpBoot [ 3 ];
    char	OEM_name [ OEM_LEN ];

    // size of a sector in bytes (usually 512) and cluster size in sectors
    // (a small power of 2, like 8 or 16).
    uint16_t	bps;
    uint8_t	spc;

    // number of reserved sectors, which defines the size of the region
    // before the allocation table.
    uint16_t	nr_reserved_secs;

    // number of allocation tables. Often 2; the second is a backup.
    uint8_t	nr_FATs;

    // number of directory slots in the root directory. Should be zero for
    // FAT32, as the root directory is not of a fixed size.
    uint16_t	root_dir_slots;

    // 16 bit count of sectors on the volume. TODO: use?
    uint16_t	nr_sectors16;

    // media descriptor. Not really used. 0xF8 is standard for fixed media.
    uint8_t	media;

    // sectors per FAT for FAT12/16. This should be zero for FAT32.
    uint16_t	FATsz16;

    // parameters for cylinder/head/sector addressing. These will not be
    // used by mfatic, as Linux uses logical addressing on block device
    // files. Note that these fields should *never* be trusted when the
    // file system is on removable media, as different hardware controllers
    // may use different drive geometries.
    uint16_t	sectors_per_track;
    uint16_t	nr_heads;

    // unused.
    uint32_t	nr_hidden_sectors;

    // 32 bit count of the number of sectors on the volume. Includes 
    // reserved sectors.
    uint32_t	nr_sectors;

    // ================================
    // FAT32 specific fields.
    //
    // 32 bit count of sectors per allocation table.
    uint32_t	sectors_per_fat;

    // extensions. Usually 0.
    uint16_t	extension_flags;

    // file system version. Must be 0 for compatibility.
    uint16_t	fsversion;

    // first cluster of the root directory. 2 is reccommended. Also the
    // sector addresses of the FSInfo sector, and an optional backup boot
    // sector (usually at sector 6).
    uint32_t	root_cluster;
    uint16_t	fsinfo_sector;
    uint16_t	boot_backup_sector;

    // reserved for future use.
    uint8_t	reserved [ 12 ];

    // BIOS drive number? Not used and should definitely not be trusted.
    uint8_t	drive_num;
    uint8_t	reserved1;

    // used by DOS when booting off a floppy. Irrelevant to us.
    uint8_t	boot_sig;

    // volume serial number, set at format time.
    uint32_t	volume_id;

    // volume label string. User defined.
    char	vol_label [ LABEL_LEN ];

    // file system type. Unknown use.
    uint8_t	fat_type;
}
__attribute__ (( packed )) fat_super_block_t;


/**
 *  file system info sector, used by FAT32. Doesn't have much info in it!
 *
 *  This struct is NOT an exact mapping of the on disk structure, as this
 *  structure ommits a large swath of reserved space, to avoid wasting
 *  memory. It therefore needs to be filled in field by field, and written
 *  back to disk field by field.
 */
typedef struct
{
    // first magic. Should be 0x41615252.
    uint32_t	magic1;

    // second magic. Should be 0x61417272.
    uint32_t	magic2;

    // number of free clusters in the volume.
    uint32_t	nr_free_clusters;

    // possibly point to first free cluster. If not, should be 0xFFFFFFFF.
    // This is not much use for us anyway, as we are using a different
    // allocation policy to the traditional driver.
    uint32_t	first_free_cluster;

    // last magic. Should be 0xAA55, same as the boot sector magic.
    uint32_t	magic3;
}
__attribute__ (( packed )) fat_fsinfo_t;


/**
 *  layout of a FAT32 directory entry.
 */
typedef struct
{
    // file name. 8 chars plus 3 char file extension.
    char	fname [ DIR_NAME_LEN ];

    // attribute bitmap. Bit meanings are defined in the constants.
    uint8_t	attributes;

    // reserved.
    uint8_t	reserved;

    // creation time.
    uint8_t	creation_tenths;
    uint16_t	creation_time;
    uint16_t	creation_date;

    // date of last access. No time is stored for this.
    uint16_t	access_date;

    // most significant bytes of the file's first cluster.
    uint16_t	cluster_msb;

    // time and date that the file was last modified.
    uint16_t	write_time;
    uint16_t	write_date;

    // least significant bytes of the file's first cluster.
    uint16_t	cluster_lsb;

    // size of the file in bytes.
    uint32_t	size;
}
__attribute__ (( packed )) fat_direntry_t;


/**
 *  This structure is specific to mfatic, and is used to locate the clusters
 *  bitmap. 
 *
 *  TODO: keep track of whether the bitmap is up to date. If the file system
 *  was last mounted by a non-mfatic driver, that driver will not have been
 *  aware of our clusters bitmap, and will not have updated it.
 */
typedef struct
{
    // sector address of the clusters bitmap, and the size of that bitmap
    // in sectors.
    uint16_t	clusters_bitmap;
    uint16_t	nr_bitmap_sectors;
}
__attribute (( packed )) mfatic_info_t;


/**
 *  This structure contains information about a mounted mfatic volume.
 *  The mfatic fuse daemon will maintain exactly one of these structures,
 *  as each instance of the daemon is responsible for servicing file
 *  requests on a single device, specified as a parameter to the mount
 *  command.
 */
typedef struct
{
    // block device file descriptor, used by the fuse daemon for reading
    // and writing on the disk itself.
    int			dev_fd;

    // permissions for accessing the block device.
    mode_t		mode;

    // user and group id's of the volumes owner.
    uid_t		uid;
    gid_t		gid;

    // pointers to in-memory copies of file system data structures.
    fat_super_block_t	*bpb;
    fat_fsinfo_t	*fsinfo;
    mfatic_info_t	*mfinfo;
}
fat_volume_t;


/**
 *  file handle structure. Stores all the information about an open file.
 *  A pointer to one of these structures is stored in fuse's fuse_file_info
 *  struct, and used by read and write to identify the file which is the
 *  target of the operation.
 */
typedef struct
{
    // pointer to the volume struct of the host file system.
    fat_volume_t	*v;

    // access mode set by open.
    int			mode;

    // current offset into the file.
    off_t		cur_offset;

    // file name. Points to a malloc()ed string.
    char		*name;

    // linked list of clusters allocated to this file. By reading the
    // entire list when the file is opened, we avoid having to repeatedly
    // seek back to the allocation table, improving performance.
    cluster_list_t	*clusters;
}
fat_file_t;


// allow clusters from the allocation table to be read into a linked list
// in memory, allowing faster access to them.
typedef struct
{
    uint32_t		cluster_id;
    cluster_list_t	*next;
}
cluster_list_t;


// Procedures exported by this header.
//
// locate a file on a device, and fill in the file struct pointed to by the
// second param. The file struct must have the volume and mode fields filled
// in before the call to this procedure.
extern int fat_open ( const char *path, fat_file_t *fd );
extern int fat_close ( fat_file_t *fd );

// read and write from a file.
extern int fat_read ( fat_file_t *fd, void *buf, size_t nbytes );
extern int fat_write ( fat_file_t *fd, const void *buf, size_t nbytes );

// change the current position in a file.
extern int fat_seek ( fat_file_t *fd, off_t offset, int whence );


#endif // MFATIC_FAT_H
