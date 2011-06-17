#/*****************************************************************************\
#*                                                                             *
#*       Copyright (c) 2003, The Regents of the University of California       *
#*     See the file COPYRIGHT for a complete copyright notice and license.     *
#*                                                                             *
#*******************************************************************************
#*
#* CVS info:
#*   $RCSfile: Makefile,v $
#*   $Revision: 1.1.1.1 $
#*   $Date: 2007/10/15 23:36:54 $
#*   $Author: rklundt $
#*
#* Purpose:
#*       Make IOR executable.
#*
#*       gmake posix      -- IOR with only POSIX interfaces
#*       gmake mpiio      -- IOR with only POSIX and MPIIO interfaces
#*       gmake hdf5       -- IOR with POSIX, MPIIO, and HDF5 interfaces
#*       gmake ncmpi      -- IOR with POSIX, MPIIO, and NCMPI interfaces
#*       gmake all        -- IOR with POSIX, MPIIO, HDF5, and NCMPI interfaces
#*       gmake clean      -- remove executable and object files
#*
#\*****************************************************************************/

include Makefile.config

# Requires GNU Make
OS=$(shell uname)

SRCS = IOR.c utilities.c parse_options.c
OBJS = $(SRCS:.c=.o)

posix: $(OBJS) aiori-POSIX.o aiori-noMPIIO.o aiori-noHDF5.o aiori-noNCMPI.o
	$(CC) -o IOR $(OBJS) \
		aiori-POSIX.o aiori-noMPIIO.o aiori-noHDF5.o aiori-noNCMPI.o \
		$(LDFLAGS) 

mpiio: $(OBJS) aiori-POSIX.o aiori-MPIIO.o aiori-noHDF5.o aiori-noNCMPI.o
	$(CC) -o IOR $(OBJS) \
		aiori-POSIX.o aiori-MPIIO.o aiori-noHDF5.o aiori-noNCMPI.o \
		$(LDFLAGS) 

hdf5: $(OBJS) aiori-POSIX.o aiori-MPIIO.o aiori-HDF5.o aiori-noNCMPI.o
	$(CC) $(LDFLAGS_HDF5) -o IOR $(OBJS) \
		aiori-POSIX.o aiori-MPIIO.o aiori-HDF5.o aiori-noNCMPI.o

ncmpi: $(OBJS) aiori-POSIX.o aiori-MPIIO.o aiori-noHDF5.o aiori-NCMPI.o
	$(CC) $(LDFLAGS_NCMPI) -o IOR $(OBJS) \
		aiori-POSIX.o aiori-MPIIO.o aiori-noHDF5.o aiori-NCMPI.o

all: $(OBJS) aiori-POSIX.o aiori-MPIIO.o aiori-HDF5.o aiori-NCMPI.o
	$(CC) $(LDFLAGS_HDF5) $(LDFLAGS_NCMPI) -o IOR $(OBJS) \
		aiori-POSIX.o aiori-MPIIO.o aiori-HDF5.o aiori-NCMPI.o

clean:
	-rm -f *.o IOR

aiori-MPIIO.o: aiori-MPIIO.c
	$(CC) -c aiori-MPIIO.c

aiori-HDF5.o: aiori-HDF5.c
	$(CC) $(CCFLAGS_HDF5) -c aiori-HDF5.c

aiori-NCMPI.o: aiori-NCMPI.c
	$(CC) $(CCFLAGS_NCMPI) -c aiori-NCMPI.c

.c.o:
	$(CC) $(CCFLAGS) -c $<
