#
# Makefile for the Emphatic FUSE daemon.
#
# Author: Matthew Signorini
#

SRC = create.c directory.c dostimes.c fat_alloc.c fileio.c inode_table.c \
      mfatic-fuse.c stat.c table.c utils.c
OBJS = $(SRC:%.c=%.o)

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O0 -g
MACROS = -D_FILE_OFFSET_BITS=64 -DPROGNAME=$(PROG) -D_GNU_SOURCE
CFLAGS += $(MACROS)

PROG = mfatic-fuse


all:		$(PROG) tags

$(PROG):	$(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)

clean:
	/bin/rm $(OBJS)

scrub:		clean
	/bin/rm $(PROG)

# Use cscope to build a tags database. If you do not have cscope installed
# at your site, you may wish to change this to invoke ctags instead.
tags:		$(SRC)
	cscope -b

# Use gcc to figure out what header files each source file depends on. The
# list of dependencies is then included in the Makefile, to ensure that
# if a header file is modified, the appropriate source files are 
# recompiled.
depend:	
	gcc $(CFLAGS) -MM $(SRC) > Depend

.PHONY:		all clean scrub tags depend


include Depend

# vim: ts=8 sw=4 noet
