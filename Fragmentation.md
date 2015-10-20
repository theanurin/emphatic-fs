It is a common misconception that file fragmentation is an inherent problem
of the FAT file system due to the design of the on-disk structures. This
is not so. This page will provide a brief overview of how file fragmentation
comes to ocurr, and also how the Emphatic FAT driver avoids it. See the
[external links](#External_Links.md) for a more in-depth discussion of the topic.

# FAT and Fragmentation #
There are two on-disk structures that we are concerned with here: the File
Allocation Table (FAT), and the data area. The data area is simply a flat
array of _clusters_, or groups of 512 byte sectors, where each cluster is
the same size, and the FAT is also an array, this time of 32 bit numbers.
Both arrays have exactly the same number of entries, so the FAT entry at
index `i` corresponds to the cluster at index `i` in the data area, ie. an
index in the FAT is _equivalent_ to an index in the data area. Finally,
each FAT entry contains the index of the next FAT index in a linked list
of FAT indices; which is effectively a linked list of cluster indices
because, as we have seen, FAT indices and cluster indices are equivalent.

The upshot of all this is that the FAT file system does not require that
files are stored in contiguous regions on the disk: even when a file's data
is scattered across the disk, the FAT driver can reassemble all the
isolated chunks by following the linked list within the allocation table.

But what about when a new file is created, I hear you ask. Obviously at
least one "free" or unallocated cluster will have to be found, and marked
as being allocated to the new file. A simplistic method of selecting a free
cluster, and the method used by most FAT implementations, is to walk
through the allocation table from the start, and as soon as an entry is
found that is marked as "free", stop, and choose that cluster for the new
file. The problem with this is that the first free cluster, where our new
file is placed, is very probably slap bang behind some other existing file,
leaving it with no room to grow.

So, I hear you ask, what if I append something to that file, what happens
then? Remember that the FAT file system does not _require_ files to be
stored in contiguous clusters, so the FAT driver will locate another free
cluster, using the same naive procedure described above, and, voila! a
fragment has been born! What is even worse about this situation is that the
new fragment will probably now be plonked right behind that file we just
created before, leaving _that_ file with no space to grow.

The naive allocation policy of choosing the first free cluster has another
more subtle flaw: what about when a file close to the end of the disk is
appended to (assuming that it is not _so_ close to the end of disk that it
would overflow). If a file has recently been deleted right at the start of
the disk, the _first_ free cluster will be near the start, and a _long_ way
from the file that needs another cluster.

# The Solution #
As I hope is clear, fragmentation is _not_ caused by the inherent design
of the structures on the disk, rather it is caused by using a really dumb
allocation policy. Emphatic attempts to minimise fragmentation by employing
a (hopefully) smarter allocation policy, to summarise:
  * New files are placed in the centre of the largest contiguous region of free clusters, ensuring that all files have as much room to grow as possible.
  * When an existing file requires another cluster, Emphatic will choose the nearest free cluster to the end of the file.

This policy obviously requires Emphatic's free space management algorithm
to keep track of where the free regions are on the disk, and which is the
largest. There are two main types of free space management techniques in
use to date: linked list, where free blocks (or clusters in our case) are
chained together, rather like files are in the FAT file system; or bitmap
management, where each block/cluster has a corresponding bit in the bitmap
which is set when the cluster is allocated to a file. The FAT file system
uses bitmap allocation, as the allocation table itself effectively
functions as a bitmap (just with 32 bits for every cluster, which are all
zero when the cluster is free).

In most cases the allocation table is much too large to store entirely in
memory: it can be up to 1 Gigabyte in size! Although we _could_ create a
separate bitmap, and perhaps keep it in the reserved sectors section of the
file system, at 128 MB this is not really any more tractable. Emphatic gets
around this by scanning through the FAT for free clusters once at mount
time, and assembling a list of (start, length) pairs describing contiguous
regions of unallocated clusters. The length of this list is linear in the
number of fragments in the file system, which is equivalent to the number
of files in the best case, that all files are stored in contiguous regions.

# External Links #
[Wikipedia page on the FAT file system, including a section on fragmentation](http://en.wikipedia.org/wiki/FAT_filesystem)