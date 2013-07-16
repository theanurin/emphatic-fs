#! /bin/bash
#
# Shell script to build Emphatic. This makes use of GNU Make, but will
# make sure that the Depend file exists before invoking make, which means
# that the Depend file does not need to be in the repository.

# create the Depend file if it does not exist, and then build the depend
# target.
touch Depend
make depend

# now build Emphatic itself.
make
