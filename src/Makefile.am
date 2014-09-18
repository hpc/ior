bin_PROGRAMS = ior
if USE_CAPS
bin_PROGRAMS += IOR
endif

ior_SOURCES  =
ior_CPPFLAGS =
ior_LDFLAGS  =
ior_LDADD    =

ior_SOURCES += ior.c utilities.c parse_options.c
ior_SOURCES += ior.h utilities.h parse_options.h aiori.h iordef.h


if USE_HDFS_AIORI
# TBD: figure out how to find this from the corresponding bin/ dir in $PATH
#      or pick an environment var to use (and set it in modulefiles)
#      or provide a config-flag, to set a variable we use here
ior_SOURCES  += aiori-HDFS.c
ior_CPPFLAGS += -I/opt/hadoop-2.2.0/include
ior_LDFLAGS  += -L/opt/hadoop-2.2.0/lib/native
ior_LDADD    += -lhdfs
endif

if USE_HDF5_AIORI
ior_SOURCES  += aiori-HDF5.c
ior_LDADD    += -lhdf5 -lz
endif

if USE_MPIIO_AIORI
ior_SOURCES += aiori-MPIIO.c
endif

if USE_NCMPI_AIORI
ior_SOURCES += aiori-NCMPI.c
ior_LDADD += -lpnetcdf
endif

if USE_PLFS_AIORI
ior_SOURCES += aiori-PLFS.c
ior_LDADD   += -lplfs
endif

if USE_POSIX_AIORI
ior_SOURCES += aiori-POSIX.c
endif


IOR_SOURCES = $(ior_SOURCES)
IOR_LDADD = $(ior_LDADD)
IOT_CPPFLAGS = $(ior_CPPFLAGS)