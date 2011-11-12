bin_PROGRAMS = ior
if USE_CAPS
bin_PROGRAMS += IOR
endif

ior_SOURCES = ior.c utilities.c parse_options.c
ior_SOURCES += ior.h utilities.h parse_options.h aiori.h iordef.h
ior_LDADD =

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

IOR_SOURCES = $(ior_SOURCES)
IOR_LDADD = $(ior_LDADD)
