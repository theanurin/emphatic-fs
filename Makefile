#
# Makefile for the Emphatic FUSE daemon.
#
# Author: Matthew Signorini
#

SRC = mfatic-fuse.c fileio.c utils.c
OBJS = $(SRC:%.c=%.o)

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -D_FILE_OFFSET_BITS=64


.PHONY:		clean scrub tags

mfatic-fuse:	$(SRC)
	$(CC) $(CFLAGS) -o mfatic-fuse $(OBJS)

clean:
	/bin/rm $(OBJS)

scrub:		clean
	/bin/rm mfatic-fuse

# Use cscope to build a tags database. If you do not have cscope installed
# at your site, you may wish to change this to invoke ctags instead.
tags:
	cscope -b

# Use gcc to figure out what header files each source file depends on. The
# list of dependencies is then included in the Makefile, to ensure that
# if a header file is modified, the appropriate source files are 
# recompiled.
Depend:		$(SRC)
	gcc $(CFLAGS) -MM $(SRC) > Depend


include Depend
