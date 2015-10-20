# Introduction #
Here is a list of things which need to be done. Some of them are pretty
urgent, and will make Emphatic _much_ nicer to use, others... maybe not
so urgent ;-)

# High Priority #
## Long file names ##
At present, Emphatic only supports DOS's traditional 8 character names
with a 3 character extension. It would be highly desirable to support
longer file names than this, in the interests of users, however there
is the question of how to do it.

VFAT style LFNs are one option, which involves adding fake directory
entries before the one belonging to the file itself, containing fragments
of the name. The advantages of this is that it maintains total
compatibility with other file system drivers, but at the expense of being
rather clumsy.

A more elegant solution would be for each directory to contain a special
string table file, which would contain file names for each file in the
directory. Each directory entry would contain an index into that string
table (perhaps in the file name field?) which would give the byte offset
of the start of the file's name string. The disadvantage is that this
would not be compatible with other drivers, and file names would appear
garbled when mounted with a different driver.

# Real Soon Now #
## Performance profiling ##
The FUSE project wiki informs us that the `getattr()` method is called
extensively during normal file system operation, and suggests that it
should be implemented as efficiently as possible; by caching results using
the path name as the key and `struct stat` as the value. The current
implementation does not use such a caching mechanism, the `struct stat`
is looked up on each invocation. It would be good to determine how much
of an impact this actually has on performance; whether or not a lot of
time is spent handling `getattr` calls.

Profiling, such as with `gprof(1)`, can do this, providing information as
to how much time is spent in each individual function, and could highlight
other areas that need optimisation.

## Directory TLB ##
While we are on the subject of performance optimisations, the path name
translation algorithm could be optimised by keeping a cache of recently
used path names. When a name is to be translated, the algorithm would
choose the longest match from the cache, and continue the usual mechanism
for the remainder of the name that did not match from the known directory.

Again, profiling will be useful in measuring the need for this, and the
benefit from it should it be implemented.