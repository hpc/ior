SUBDIRS = . test

bin_PROGRAMS = ior mdtest md-workbench
if USE_CAPS
bin_PROGRAMS += IOR MDTEST MD-WORKBENCH
endif

noinst_HEADERS = ior.h utilities.h parse_options.h aiori.h iordef.h ior-internal.h option.h mdtest.h aiori-debug.h aiori-POSIX.h md-workbench.h

lib_LIBRARIES = libaiori.a
libaiori_a_SOURCES = ior.c mdtest.c utilities.c parse_options.c ior-output.c option.c md-workbench.c

extraSOURCES = aiori.c aiori-DUMMY.c
extraLDADD =
extraLDFLAGS =
extraCPPFLAGS =

md_workbench_SOURCES = md-workbench-main.c
md_workbench_LDFLAGS =
md_workbench_LDADD = libaiori.a
md_workbench_CPPFLAGS =

ior_SOURCES = ior-main.c
ior_LDFLAGS =
ior_LDADD = libaiori.a
ior_CPPFLAGS =

mdtest_SOURCES = mdtest-main.c
mdtest_LDFLAGS =
mdtest_LDADD = libaiori.a
mdtest_CPPFLAGS =

if USE_HDFS_AIORI
# TBD: figure out how to find the appropriate -I and -L dirs.  Maybe we can
#      get them from the corresponding bin/ dir in $PATH, or pick an
#      environment var to use (and set it in modulefiles), or provide a
#      config-flag, to set a variable we use here.
extraSOURCES  += aiori-HDFS.c
extraCPPFLAGS += -I/opt/hadoop-2.2.0/include
extraLDFLAGS  += -L/opt/hadoop-2.2.0/lib/native
extraLDADD    += -lhdfs
endif

if HAVE_CUDA
extraLDADD    += -lcudart
endif

if HAVE_GPU_DIRECT
extraLDADD    += -lcufile -lstdc++
extraSOURCES  += utilities-gpu.cu
endif

if USE_HDF5_AIORI
extraSOURCES  += aiori-HDF5.c
extraLDADD    += -lhdf5 -lz
endif

if USE_IME_AIORI
extraSOURCES  += aiori-IME.c
extraCPPFLAGS += -I/opt/ddn/ime/include
extraLDFLAGS  += -L/opt/ddn/ime/lib
extraLDADD    += -lim_client
endif

if USE_MPIIO_AIORI
extraSOURCES += aiori-MPIIO.c
endif

if USE_NCMPI_AIORI
extraSOURCES += aiori-NCMPI.c
extraLDADD   += -lpnetcdf
endif

if USE_MMAP_AIORI
extraSOURCES += aiori-MMAP.c
endif

if USE_POSIX_AIORI
extraSOURCES += aiori-POSIX.c
endif

if USE_AIO_AIORI
extraSOURCES += aiori-aio.c
extraLDADD    += -laio
endif

if USE_PMDK_AIORI
extraSOURCES += aiori-PMDK.c
extraLDADD   += -lpmem
endif

if USE_RADOS_AIORI
extraSOURCES += aiori-RADOS.c
extraLDADD += -lrados
endif

if USE_CEPHFS_AIORI
extraSOURCES += aiori-CEPHFS.c
extraLDADD += -lcephfs
endif

if USE_LIBNFS_AIORI
extraSOURCES += aiori-LIBNFS.c
extraLDADD += -lnfs
endif

if USE_DAOS_AIORI
extraSOURCES += aiori-DFS.c
extraLDADD   += -lgurt -ldaos_common -ldaos -ldfs -luuid
endif

if USE_GFARM_AIORI
extraSOURCES += aiori-Gfarm.c
extraLDADD += -lgfarm
endif

if USE_CHFS_AIORI
extraSOURCES += aiori-CHFS.c
extraCPPFLAGS += @CHFS_CFLAGS@
extraLDFLAGS += @CHFS_RPATH@
extraLDADD += @CHFS_LIBS@
endif

if USE_FINCHFS_AIORI
extraSOURCES += aiori-FINCHFS.c
extraCPPFLAGS += @FINCHFS_CFLAGS@
extraLDFLAGS += @FINCHFS_RPATH@
extraLDADD += @FINCHFS_LIBS@
endif

if USE_S3_4C_AIORI
extraSOURCES  += aiori-S3-4c.c
if AWS4C_DIR
  extraCPPFLAGS += $(AWS4C_CPPFLAGS)
  extraLDFLAGS  += $(AWS4C_LDFLAGS)
endif
extraLDADD    += -lcurl
extraLDADD    += -lxml2
extraLDADD    += -laws4c
extraLDADD    += -laws4c_extra
extraLDADD    += -lcrypto
endif

if USE_S3_LIBS3_AIORI
extraSOURCES  += aiori-S3-libs3.c
extraLDADD    += -ls3
endif

if WITH_LUSTRE
extraLDADD  += -llustreapi
endif

ior_SOURCES  += $(extraSOURCES)
ior_LDFLAGS  += $(extraLDFLAGS)
ior_LDADD    += $(extraLDADD)
ior_CPPFLAGS += $(extraCPPFLAGS)

mdtest_SOURCES  += $(extraSOURCES)
mdtest_LDFLAGS  += $(extraLDFLAGS)
mdtest_LDADD    += $(extraLDADD)
mdtest_CPPFLAGS += $(extraCPPFLAGS)

md_workbench_SOURCES  += $(extraSOURCES)
md_workbench_LDFLAGS  += $(extraLDFLAGS)
md_workbench_LDADD    += $(extraLDADD)
md_workbench_CPPFLAGS += $(extraCPPFLAGS)

MD_WORKBENCH_SOURCES  = $(md_workbench_SOURCES)
MD_WORKBENCH_LDFLAGS  = $(md_workbench_LDFLAGS)
MD_WORKBENCH_LDADD    = $(md_workbench_LDADD)
MD_WORKBENCH_CPPFLAGS = $(md_workbench_CPPFLAGS)

IOR_SOURCES  = $(ior_SOURCES)
IOR_LDFLAGS  = $(ior_LDFLAGS)
IOR_LDADD    = $(ior_LDADD)
IOR_CPPFLAGS = $(ior_CPPFLAGS)

MDTEST_SOURCES  = $(mdtest_SOURCES)
MDTEST_LDFLAGS  = $(mdtest_LDFLAGS)
MDTEST_LDADD    = $(mdtest_LDADD)
MDTEST_CPPFLAGS = $(mdtest_CPPFLAGS)

libaiori_a_SOURCES += $(extraSOURCES)
libaiori_a_CPPFLAGS = $(extraCPPFLAGS)

# Generate a config file with the build flags to allow the reuse of library
.PHONY: build.conf
all-local: build.conf
build.conf:
	@echo LDFLAGS=$(LDFLAGS) $(extraLDFLAGS) $(extraLDADD) $(LIBS) > build.conf
	@echo CFLAGS=$(CFLAGS) $(extraCPPFLAGS) >> build.conf

.cu.o:
	$(NVCC) $(CFLAGS) -c -o $@ $<
