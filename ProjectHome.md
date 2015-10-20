# Overview #
Emphatic is an improved driver for Fat32 file systems, which aims to minimise file fragmentation by using a smarter allocation policy (see the [wiki](Fragmentation.md) for details on how this is achieved).

It is implemented using FUSE (file system in user space), which means the file system driver runs as a daemon, as opposed to a kernel module, and as such is a simpler and more robust program (ie. if it crashes, it will dump core, rather than BSOD). See [wikipedia](http://en.wikipedia.org/wiki/Filesystem_in_Userspace) for a brief overview of FUSE, or follow the link on the left hand side to the FUSE project's home page for more details.

FUSE is supported natively on Linux, however there are a number of FUSE ports to other platforms, including Microsoft Windows and OSX.

# Project Status #
Alpha release. This means that the program builds, but is still being tested, to isolate and remove bugs.