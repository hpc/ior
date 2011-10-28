SUBDIRS = cbif

bin_PROGRAMS = ior

ior_SOURCES = ior.c utilities.c parse_options.c
ior_SOURCES += ior.h aiori.h iordef.h defaults.h
ior_LDADD = -lm

if USE_POSIX_AIORI
ior_SOURCES += aiori-POSIX.c
endif
if USE_MPIIO_AIORI
ior_SOURCES += aiori-MPIIO.c
endif
if USE_HDF5_AIORI
ior_SOURCES += aiori-HDF5.c
ior_LDADD += -lhdf5 -lz
endif
if USE_NCMPI_AIORI
ior_SOURCES += aiori-NCMPI.c
ior_LDADD += -lpnetcdf
endif
