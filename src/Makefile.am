SUBDIRS = cbif

bin_PROGRAMS = ior

# N.B. "NCMPI" is an outdated name for pnetcdf (parallel netcdf)
ior_SOURCES = IOR.c utilities.c parse_options.c \
	IOR.h aiori.h iordef.h defaults.h IOR-aiori.h \
	aiori-POSIX.c aiori-noMPIIO.c aiori-noHDF5.c aiori-noNCMPI.c 
ior_CFLAGS = 
ior_LDADD = -lm
