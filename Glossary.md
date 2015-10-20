This page enumerates definitions of some terms which are used throughout
the wiki.

# On-Disk Structures #

## Bios Parameter Block (BPB) ##
The very first block of the disk partition, which contains important
information about the whole file system. We will often refer to this as
the super-block, following Unix terminology.

## File Allocation Table (FAT) ##
A flat array of 32 bit numbers, where the FAT entry at index `i` stores
either the index of the next cluster in the chain, a reserved marker (or
sentinel) if cluster `i` is free or bad, or an end of file sentinel if
cluster `i` is the last in the chain.

Note that the most significant 4 bits of each FAT entry are **not used**,
and are to be masked out.

## Data Area ##
The region on the disk from the end of the FAT(s) to the end of the
partition. This space can be represented as an array of clusters.

## Sector ##
512 bytes. At the level of the disk hardware, this is the unit by which
the disk is addressed (ie. at the lowest level, reads or writes are done
in units of sectors). Bear in mind that the low level intricacies of the
disk hardware is (fortunately) abstracted away by the device file, which
allows the FUSE daemon to treat the disk as an array of bytes, rather
than sectors.

## Clusters ##
A group of sectors. For a given file system, all clusters are the same
size, and all are contiguous, so the data area on the disk may be viewed
as a flat array of clusters.