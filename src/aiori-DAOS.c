/*
 * -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/*
 * SPECIAL LICENSE RIGHTS-OPEN SOURCE SOFTWARE
 * The Government's rights to use, modify, reproduce, release, perform, display,
 * or disclose this software are subject to the terms of Contract No. B599860,
 * and the terms of the GNU General Public License version 2.
 * Any reproduction of computer software, computer software documentation, or
 * portions thereof marked with this legend must also reproduce the markings.
 */
/*
 * Copyright (c) 2013, 2016 Intel Corporation.
 */
/*
 * This file implements the abstract I/O interface for DAOS.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <libgen.h>
#include <stdbool.h>
#include <daos.h>
#include <daos_types.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "list.h"

/************************** O P T I O N S *****************************/
struct daos_options{
        char		*daosPool;
        char		*daosPoolSvc;
        char		*daosGroup;
	int		daosRecordSize;
	int		daosStripeSize;
	uint64_t	daosStripeCount;
	uint64_t	daosStripeMax;	/* max length of a stripe */
	int		daosAios;	/* max number of concurrent async I/Os */
	int		daosWriteOnly;	/* write only, no flush and commit */
	uint64_t	daosEpoch;	/* epoch to access */
	uint64_t	daosWait;	/* epoch to wait for before reading */
	int		daosKill;	/* kill a target while running IOR */
	char		*daosObjectClass; /* object class */
};

static struct daos_options o = {
        .daosPool		= NULL,
        .daosPoolSvc		= NULL,
        .daosGroup		= NULL,
	.daosRecordSize		= 262144,
	.daosStripeSize		= 524288,
	.daosStripeCount	= -1,
	.daosStripeMax		= 0,
	.daosAios		= 1,
	.daosWriteOnly		= 0,
	.daosEpoch		= 0,
	.daosWait		= 0,
	.daosKill		= 0,
	.daosObjectClass	= NULL,
};

static option_help options [] = {
      {'p', "daosPool", "pool uuid", OPTION_REQUIRED_ARGUMENT, 's', &o.daosPool},
      {'v', "daosPoolSvc", "pool SVCL", OPTION_REQUIRED_ARGUMENT, 's', &o.daosPoolSvc},
      {'g', "daosGroup", "server group", OPTION_OPTIONAL_ARGUMENT, 's', &o.daosGroup},
      {'r', "daosRecordSize", "Record Size", OPTION_OPTIONAL_ARGUMENT, 'd', &o.daosRecordSize},
      {'s', "daosStripeSize", "Stripe Size", OPTION_OPTIONAL_ARGUMENT, 'd', &o.daosStripeSize},
      {'c', "daosStripeCount", "Stripe Count", OPTION_OPTIONAL_ARGUMENT, 'u', &o.daosStripeCount},
      {'m', "daosStripeMax", "Max Stripe",OPTION_OPTIONAL_ARGUMENT, 'u', &o.daosStripeMax},
      {'a', "daosAios", "Concurrent Async IOs",OPTION_OPTIONAL_ARGUMENT, 'd', &o.daosAios},
      {'w', "daosWriteOnly", "Write Only, no commit",OPTION_OPTIONAL_ARGUMENT, 'd', &o.daosWriteOnly},
      {'e', "daosEpoch", "Epoch Number to Access",OPTION_OPTIONAL_ARGUMENT, 'u', &o.daosEpoch},
      {'t', "daosWait", "Epoch to wait for before read",OPTION_OPTIONAL_ARGUMENT, 'u', &o.daosWait},
      {'k', "daosKill", "Kill target while running",OPTION_OPTIONAL_ARGUMENT, 'd', &o.daosKill},
      {'o', "daosObjectClass", "object class", OPTION_OPTIONAL_ARGUMENT, 's', &o.daosObjectClass},
      LAST_OPTION
};

/**************************** P R O T O T Y P E S *****************************/

static void DAOS_Init(IOR_param_t *);
static void DAOS_Fini(IOR_param_t *);
static void *DAOS_Create(char *, IOR_param_t *);
static void *DAOS_Open(char *, IOR_param_t *);
static IOR_offset_t DAOS_Xfer(int, void *, IOR_size_t *,
                              IOR_offset_t, IOR_param_t *);
static void DAOS_Close(void *, IOR_param_t *);
static void DAOS_Delete(char *, IOR_param_t *);
static char* DAOS_GetVersion();
static void DAOS_Fsync(void *, IOR_param_t *);
static IOR_offset_t DAOS_GetFileSize(IOR_param_t *, MPI_Comm, char *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t daos_aiori = {
        .name = "DAOS",
        .create = DAOS_Create,
        .open = DAOS_Open,
        .xfer = DAOS_Xfer,
        .close = DAOS_Close,
        .delete = DAOS_Delete,
        .get_version = DAOS_GetVersion,
        .fsync = DAOS_Fsync,
        .get_file_size = DAOS_GetFileSize,
        .initialize = DAOS_Init,
        .finalize = DAOS_Fini,
};

enum handleType {
        POOL_HANDLE,
        CONTAINER_HANDLE
};

struct fileDescriptor {
        daos_handle_t    container;
        daos_cont_info_t containerInfo;
        daos_handle_t    object;
        daos_epoch_t     epoch;
};

struct aio {
        cfs_list_t              a_list;
        char                    a_dkeyBuf[32];
        daos_key_t              a_dkey;
        daos_recx_t             a_recx;
        unsigned char           a_csumBuf[32];
        daos_csum_buf_t         a_csum;
        daos_epoch_range_t      a_epochRange;
        daos_iod_t              a_iod;
        daos_iov_t              a_iov;
        daos_sg_list_t          a_sgl;
        struct daos_event       a_event;
};

static daos_handle_t       eventQueue;
static struct daos_event **events;
static unsigned char      *buffers;
static int                 nAios;
static daos_handle_t       pool;
static daos_pool_info_t    poolInfo;
static daos_oclass_id_t    objectClass = DAOS_OC_LARGE_RW;
static CFS_LIST_HEAD(aios);
static IOR_offset_t        total_size;

/***************************** F U N C T I O N S ******************************/

/* For DAOS methods. */
#define DCHECK(rc, format, ...)                                         \
do {                                                                    \
        int _rc = (rc);                                                 \
                                                                        \
        if (_rc < 0) {                                                  \
                fprintf(stdout, "ior ERROR (%s:%d): %d: %d: "           \
                        format"\n", __FILE__, __LINE__, rank, _rc,      \
                        ##__VA_ARGS__);                                 \
                fflush(stdout);                                         \
                MPI_Abort(MPI_COMM_WORLD, -1);                          \
        }                                                               \
} while (0)

#define INFO(level, param, format, ...)                                 \
do {                                                                    \
        if (param->verbose >= level)                                    \
                printf("[%d] "format"\n", rank, ##__VA_ARGS__);         \
} while (0)

/* For generic errors like invalid command line options. */
#define GERR(format, ...)                                               \
do {                                                                    \
        fprintf(stdout, format"\n", ##__VA_ARGS__);                     \
        MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");  \
} while (0)

/* Distribute process 0's pool or container handle to others. */
static void HandleDistribute(daos_handle_t *handle, enum handleType type,
                             IOR_param_t *param)
{
        daos_iov_t global;
        int        rc;

        assert(type == POOL_HANDLE || !daos_handle_is_inval(pool));

        global.iov_buf = NULL;
        global.iov_buf_len = 0;
        global.iov_len = 0;

        if (rank == 0) {
                /* Get the global handle size. */
                if (type == POOL_HANDLE)
                        rc = daos_pool_local2global(*handle, &global);
                else
                        rc = daos_cont_local2global(*handle, &global);
                DCHECK(rc, "Failed to get global handle size");
        }

        MPI_CHECK(MPI_Bcast(&global.iov_buf_len, 1, MPI_UINT64_T, 0,
                            param->testComm),
                  "Failed to bcast global handle buffer size");

        global.iov_buf = malloc(global.iov_buf_len);
        if (global.iov_buf == NULL)
                ERR("Failed to allocate global handle buffer");

        if (rank == 0) {
                if (type == POOL_HANDLE)
                        rc = daos_pool_local2global(*handle, &global);
                else
                        rc = daos_cont_local2global(*handle, &global);
                DCHECK(rc, "Failed to create global handle");
        }

        MPI_CHECK(MPI_Bcast(global.iov_buf, global.iov_buf_len, MPI_BYTE, 0,
                            param->testComm),
                  "Failed to bcast global pool handle");

        if (rank != 0) {
                /* A larger-than-actual length works just fine. */
                global.iov_len = global.iov_buf_len;

                if (type == POOL_HANDLE)
                        rc = daos_pool_global2local(global, handle);
                else
                        rc = daos_cont_global2local(pool, global, handle);
                DCHECK(rc, "Failed to get local handle");
        }

        free(global.iov_buf);
}

static void ContainerOpen(char *testFileName, IOR_param_t *param,
                          daos_handle_t *container, daos_cont_info_t *info)
{
        int rc;

        if (rank == 0) {
                uuid_t       uuid;
                unsigned int dFlags;

                rc = uuid_parse(testFileName, uuid);
                DCHECK(rc, "Failed to parse 'testFile': %s", testFileName);

                if (param->open == WRITE &&
                    param->useExistingTestFile == FALSE) {
                        INFO(VERBOSE_2, param, "Creating container %s",
                             testFileName);

                        rc = daos_cont_create(pool, uuid, NULL /* ev */);
                        DCHECK(rc, "Failed to create container %s",
                               testFileName);
                }

                INFO(VERBOSE_2, param, "Openning container %s", testFileName);

                if (param->open == WRITE)
                        dFlags = DAOS_COO_RW;
                else
                        dFlags = DAOS_COO_RO;

                rc = daos_cont_open(pool, uuid, dFlags, container, info,
                                    NULL /* ev */);
                DCHECK(rc, "Failed to open container %s", testFileName);

                INFO(VERBOSE_2, param, "Container epoch state:");
                INFO(VERBOSE_2, param, "   HCE: %lu",
                     info->ci_epoch_state.es_hce);
                INFO(VERBOSE_2, param, "   LRE: %lu",
                     info->ci_epoch_state.es_lre);
                INFO(VERBOSE_2, param, "   LHE: %lu (%lx)",
                     info->ci_epoch_state.es_lhe, info->ci_epoch_state.es_lhe);
                INFO(VERBOSE_2, param, "  GHCE: %lu",
                     info->ci_epoch_state.es_ghce);
                INFO(VERBOSE_2, param, "  GLRE: %lu",
                     info->ci_epoch_state.es_glre);
                INFO(VERBOSE_2, param, " GHPCE: %lu",
                     info->ci_epoch_state.es_ghpce);

#if 0
                if (param->open != WRITE && o.daosWait != 0) {
                        daos_epoch_t e;

                        e = o.daosWait;

                        INFO(VERBOSE_2, param, "Waiting for epoch %lu", e);

                        rc = daos_epoch_wait(*container, &e,
                                             NULL /* ignore HLE */,
                                             NULL /* synchronous */);
                        DCHECK(rc, "Failed to wait for epoch %lu",
                               o.daosWait);
                }

                if (param->open == WRITE &&
                    param->useExistingTestFile == FALSE) {
                        daos_oclass_attr_t attr = {
                                .ca_schema              = DAOS_OS_STRIPED,
                                .ca_resil_degree        = 0,
                                .ca_resil               = DAOS_RES_REPL,
                                .ca_grp_nr              = 4,
                                .u.repl                 = {
                                        .r_method       = 0,
                                        .r_num          = 2
                                }
                        };

                        INFO(VERBOSE_2, param, "Registering object class");

                        rc = daos_oclass_register(container, objectClass, &attr,
                                                  NULL /* ev */);
                        DCHECK(rc, "Failed to register object class");
                }
#endif
        }

        HandleDistribute(container, CONTAINER_HANDLE, param);

        MPI_CHECK(MPI_Bcast(info, sizeof *info, MPI_BYTE, 0, param->testComm),
                  "Failed to broadcast container info");
}

static void ContainerClose(daos_handle_t container, IOR_param_t *param)
{
        int rc;

        if (rank != 0) {
                rc = daos_cont_close(container, NULL /* ev */);
                DCHECK(rc, "Failed to close container");
        }

        /* An MPI_Gather() call would probably be more efficient. */
        MPI_CHECK(MPI_Barrier(param->testComm),
                  "Failed to synchronize processes");

        if (rank == 0) {
                rc = daos_cont_close(container, NULL /* ev */);
                DCHECK(rc, "Failed to close container");
        }
}

static void ObjectOpen(daos_handle_t container, daos_handle_t *object,
                       daos_epoch_t epoch, IOR_param_t *param)
{
        daos_obj_id_t oid;
        unsigned int  flags;
        int           rc;

        oid.hi = 0;
        oid.lo = 1;
        daos_obj_id_generate(&oid, 0, objectClass);

#if 0
        /** declaring object not implemented commenting it */
        if (rank == 0 && param->open == WRITE &&
            param->useExistingTestFile == FALSE) {
                INFO(VERBOSE_2, param, "Declaring object");

                rc = daos_obj_declare(container, oid, epoch, NULL /* oa */,
                                      NULL /* ev */);
                DCHECK(rc, "Failed to declare object");
        }
#endif
        /* An MPI_Bcast() call would probably be more efficient. */
        MPI_CHECK(MPI_Barrier(param->testComm),
                  "Failed to synchronize processes");

        if (param->open == WRITE)
                flags = DAOS_OO_RW;
        else
                flags = DAOS_OO_RO;

        rc = daos_obj_open(container, oid, epoch, flags, object, NULL /* ev */);
        DCHECK(rc, "Failed to open object");
}

static void ObjectClose(daos_handle_t object)
{
        int rc;

        rc = daos_obj_close(object, NULL /* ev */);
        DCHECK(rc, "Failed to close object");
}

static void AIOInit(IOR_param_t *param)
{
        struct aio *aio;
        int         i;
        int         rc;

        rc = posix_memalign((void **) &buffers, sysconf(_SC_PAGESIZE),
                            param->transferSize * o.daosAios);
        DCHECK(rc, "Failed to allocate buffer array");

        for (i = 0; i < o.daosAios; i++) {
                aio = malloc(sizeof *aio);
                if (aio == NULL)
                        ERR("Failed to allocate aio array");

                memset(aio, 0, sizeof *aio);

                aio->a_dkey.iov_buf     = aio->a_dkeyBuf;
                aio->a_dkey.iov_buf_len = sizeof aio->a_dkeyBuf;

                aio->a_recx.rx_nr       = 1;

                aio->a_csum.cs_csum     = &aio->a_csumBuf;
                aio->a_csum.cs_buf_len  = sizeof aio->a_csumBuf;
                aio->a_csum.cs_len      = aio->a_csum.cs_buf_len;

                aio->a_epochRange.epr_hi = DAOS_EPOCH_MAX;

                aio->a_iod.iod_name.iov_buf = "data";
                aio->a_iod.iod_name.iov_buf_len =
                        strlen(aio->a_iod.iod_name.iov_buf) + 1;
                aio->a_iod.iod_name.iov_len = aio->a_iod.iod_name.iov_buf_len;
                aio->a_iod.iod_nr = 1;
                aio->a_iod.iod_type  = DAOS_IOD_ARRAY;
                aio->a_iod.iod_recxs = &aio->a_recx;
                aio->a_iod.iod_csums = &aio->a_csum;
                aio->a_iod.iod_eprs  = &aio->a_epochRange;
                aio->a_iod.iod_size  = param->transferSize;

                aio->a_iov.iov_buf = buffers + param->transferSize * i;
                aio->a_iov.iov_buf_len = param->transferSize;
                aio->a_iov.iov_len = aio->a_iov.iov_buf_len;

                aio->a_sgl.sg_nr = 1;
                aio->a_sgl.sg_iovs = &aio->a_iov;

                rc = daos_event_init(&aio->a_event, eventQueue,
                                     NULL /* parent */);
                DCHECK(rc, "Failed to initialize event for aio[%d]", i);

                cfs_list_add(&aio->a_list, &aios);

                INFO(VERBOSE_3, param, "Allocated AIO %p: buffer %p", aio,
                     aio->a_iov.iov_buf);
        }

        nAios = o.daosAios;

        events = malloc((sizeof *events) * o.daosAios);
        if (events == NULL)
                ERR("Failed to allocate events array");
}

static void AIOFini(IOR_param_t *param)
{
        struct aio *aio;
        struct aio *tmp;

        free(events);

        cfs_list_for_each_entry_safe(aio, tmp, &aios, a_list) {
                INFO(VERBOSE_3, param, "Freeing AIO %p: buffer %p", aio,
                     aio->a_iov.iov_buf);
                cfs_list_del_init(&aio->a_list);
                daos_event_fini(&aio->a_event);
                free(aio);
        }

        free(buffers);
}

static void AIOWait(IOR_param_t *param)
{
        struct aio *aio;
        int         i;
        int         rc;

        rc = daos_eq_poll(eventQueue, 0, DAOS_EQ_WAIT, o.daosAios,
                          events);
        DCHECK(rc, "Failed to poll event queue");
        assert(rc <= o.daosAios - nAios);

        for (i = 0; i < rc; i++) {
                int ret;

                aio = (struct aio *)
                      ((char *) events[i] -
                       (char *) (&((struct aio *) 0)->a_event));

                DCHECK(aio->a_event.ev_error, "Failed to transfer (%lu, %lu)",
                       aio->a_iod.iod_recxs->rx_idx,
                       aio->a_iod.iod_recxs->rx_nr);

                daos_event_fini(&aio->a_event);
                ret = daos_event_init(&aio->a_event, eventQueue,
                                      NULL /* parent */);
                DCHECK(ret, "Failed to reinitialize event for AIO %p", aio);

                cfs_list_move(&aio->a_list, &aios);
                nAios++;

                if (param->verbose >= VERBOSE_3)
                INFO(VERBOSE_3, param, "Completed AIO %p: buffer %p", aio,
                     aio->a_iov.iov_buf);
        }

        INFO(VERBOSE_3, param, "Found %d completed AIOs (%d free %d busy)", rc,
             nAios, o.daosAios - nAios);
}

static void ObjectClassParse(const char *string)
{
        if (strcasecmp(string, "tiny") == 0)
                objectClass = DAOS_OC_TINY_RW;
        else if (strcasecmp(string, "small") == 0)
                objectClass = DAOS_OC_SMALL_RW;
        else if (strcasecmp(string, "large") == 0)
                objectClass = DAOS_OC_LARGE_RW;
        else if (strcasecmp(string, "echo") == 0)
                objectClass = DAOS_OC_ECHO_RW;
        else if (strcasecmp(string, "R2") == 0)
                objectClass = DAOS_OC_R2_RW;
        else if (strcasecmp(string, "R2S") == 0)
                objectClass = DAOS_OC_R2S_RW;
        else if (strcasecmp(string, "R3S") == 0)
                objectClass = DAOS_OC_R3S_RW;
        else if (strcasecmp(string, "R3") == 0)
                objectClass = DAOS_OC_R3_RW;
        else if (strcasecmp(string, "R4") == 0)
                objectClass = DAOS_OC_R4_RW;
        else if (strcasecmp(string, "R4S") == 0)
                objectClass = DAOS_OC_R4S_RW;
        else if (strcasecmp(string, "repl_max") == 0)
                objectClass = DAOS_OC_REPL_MAX_RW;
        else
                GERR("Invalid 'daosObjectClass' argument: '%s'", string);
}

static const char *GetGroup(IOR_param_t *param)
{
        if (strlen(o.daosGroup) == 0)
                return NULL;
        return o.daosGroup;
}

static void ParseService(IOR_param_t *param, int max, d_rank_list_t *ranks)
{
        char *s;

        s = strdup(o.daosPoolSvc);
        if (s == NULL)
                GERR("failed to duplicate argument");
        ranks->rl_nr = 0;
        while ((s = strtok(s, ":")) != NULL) {
                if (ranks->rl_nr >= max) {
                        free(s);
                        GERR("at most %d pool service replicas supported", max);
                }
                ranks->rl_ranks[ranks->rl_nr] = atoi(s);
                ranks->rl_nr++;
                s = NULL;
        }
        free(s);
}

static void DAOS_Init(IOR_param_t *param)
{
        int rc;

        if (strlen(o.daosObjectClass) != 0)
                ObjectClassParse(o.daosObjectClass);

        if (param->filePerProc)
                GERR("'filePerProc' not yet supported");
        if (o.daosStripeMax % o.daosStripeSize != 0)
                GERR("'daosStripeMax' must be a multiple of 'daosStripeSize'");
        if (o.daosStripeSize % param->transferSize != 0)
                GERR("'daosStripeSize' must be a multiple of 'transferSize'");
        if (param->transferSize % o.daosRecordSize != 0)
                GERR("'transferSize' must be a multiple of 'daosRecordSize'");
        if (o.daosKill && ((objectClass != DAOS_OC_R2_RW) ||
                                (objectClass != DAOS_OC_R3_RW) ||
                                (objectClass != DAOS_OC_R4_RW) ||
                                (objectClass != DAOS_OC_R2S_RW) ||
                                (objectClass != DAOS_OC_R3S_RW) ||
                                (objectClass != DAOS_OC_R4S_RW) ||
                                (objectClass != DAOS_OC_REPL_MAX_RW)))
                GERR("'daosKill' only makes sense with 'daosObjectClass=repl'");

        if (rank == 0)
                INFO(VERBOSE_0, param, "WARNING: USING daosStripeMax CAUSES READS TO RETURN INVALID DATA");

        rc = daos_init();
        DCHECK(rc, "Failed to initialize daos");

        rc = daos_eq_create(&eventQueue);
        DCHECK(rc, "Failed to create event queue");

        if (rank == 0) {
                uuid_t           uuid;
                d_rank_t         rank[13];
                d_rank_list_t    ranks;

                if (strlen(o.daosPool) == 0)
                        GERR("'daosPool' must be specified");
                if (strlen(o.daosPoolSvc) == 0)
                        GERR("'daosPoolSvc' must be specified");

                INFO(VERBOSE_2, param, "Connecting to pool %s %s",
                     o.daosPool, o.daosPoolSvc);

                rc = uuid_parse(o.daosPool, uuid);
                DCHECK(rc, "Failed to parse 'daosPool': %s", o.daosPool);
                ranks.rl_ranks = rank;
                ParseService(param, sizeof(rank) / sizeof(rank[0]), &ranks);

                rc = daos_pool_connect(uuid, GetGroup(param), &ranks,
                                       DAOS_PC_RW, &pool, &poolInfo,
                                       NULL /* ev */);
                DCHECK(rc, "Failed to connect to pool %s", o.daosPool);
        }

        HandleDistribute(&pool, POOL_HANDLE, param);

        MPI_CHECK(MPI_Bcast(&poolInfo, sizeof poolInfo, MPI_BYTE, 0,
                            param->testComm),
                  "Failed to bcast pool info");

        if (o.daosStripeCount == -1)
                o.daosStripeCount = poolInfo.pi_ntargets * 64UL;
}

static void DAOS_Fini(IOR_param_t *param)
{
        int rc;

	rc = daos_pool_disconnect(pool, NULL /* ev */);
	DCHECK(rc, "Failed to disconnect from pool %s", o.daosPool);

        rc = daos_eq_destroy(eventQueue, 0 /* flags */);
        DCHECK(rc, "Failed to destroy event queue");

        rc = daos_fini();
        DCHECK(rc, "Failed to finalize daos");
}

static void *DAOS_Create(char *testFileName, IOR_param_t *param)
{
        return DAOS_Open(testFileName, param);
}

static void *DAOS_Open(char *testFileName, IOR_param_t *param)
{
        struct fileDescriptor *fd;
        daos_epoch_t           ghce;

        fd = malloc(sizeof *fd);
        if (fd == NULL)
                ERR("Failed to allocate fd");

        ContainerOpen(testFileName, param, &fd->container, &fd->containerInfo);

        ghce = fd->containerInfo.ci_epoch_state.es_ghce;
        if (param->open == WRITE) {
                if (o.daosEpoch == 0)
                        fd->epoch = ghce + 1;
                else if (o.daosEpoch <= ghce)
                        GERR("Can't modify committed epoch\n");
                else
                        fd->epoch = o.daosEpoch;
        } else {
                if (o.daosEpoch == 0) {
                        if (o.daosWait == 0)
                                fd->epoch = ghce;
                        else
                                fd->epoch = o.daosWait;
                } else if (o.daosEpoch > ghce) {
                        GERR("Can't read uncommitted epoch\n");
                } else {
                        fd->epoch = o.daosEpoch;
                }
        }

        if (rank == 0)
                INFO(VERBOSE_2, param, "Accessing epoch %lu", fd->epoch);

        if (rank == 0 && param->open == WRITE) {
                daos_epoch_t e = fd->epoch;
                int          rc;

                INFO(VERBOSE_2, param, "Holding epoch %lu", fd->epoch);

                rc = daos_epoch_hold(fd->container, &fd->epoch,
                                     NULL /* state */, NULL /* ev */);
                DCHECK(rc, "Failed to hold epoch");
                assert(fd->epoch == e);
        }

        ObjectOpen(fd->container, &fd->object, fd->epoch, param);

        AIOInit(param);

        return fd;
}

static void
kill_daos_server(IOR_param_t *param)
{
	daos_pool_info_t        info;
	d_rank_t                rank, svc_ranks[13];
	d_rank_list_t           svc, targets;
        uuid_t                  uuid;
        char                    *s;
        int                     rc;

	rc = daos_pool_query(pool, NULL, &info, NULL);
	DCHECK(rc, "Error in querying pool\n");

	if (info.pi_ntargets - info.pi_ndisabled <= 1)
		return;
	/* choose the last alive one */
	rank = info.pi_ntargets - 1 - info.pi_ndisabled;

        rc = uuid_parse(o.daosPool, uuid);
        DCHECK(rc, "Failed to parse 'daosPool': %s", o.daosPool);

        if (rc != 0)
	printf("Killing tgt rank: %d (total of %d of %d already disabled)\n",
	       rank,  info.pi_ndisabled, info.pi_ntargets);
	fflush(stdout);

	rc = daos_mgmt_svc_rip(GetGroup(param), rank, true, NULL);
	DCHECK(rc, "Error in killing server\n");

	targets.rl_nr = 1;
	targets.rl_ranks = &rank;

        svc.rl_ranks = svc_ranks;
        ParseService(param, sizeof(svc_ranks)/ sizeof(svc_ranks[0]), &svc);

	rc = daos_pool_exclude(uuid, NULL, &svc, &targets, NULL);
	DCHECK(rc, "Error in excluding pool from poolmap\n");

        rc = daos_pool_query(pool, NULL, &info, NULL);
	DCHECK(rc, "Error in querying pool\n");

        printf("%d targets succesfully disabled\n",
               info.pi_ndisabled);

}

static void
kill_and_sync(IOR_param_t *param)
{
        double start, end;

        start = MPI_Wtime();
        if (rank == 0)
                kill_daos_server(param);

        if (rank == 0)
                printf("Done killing and excluding\n");

        MPI_CHECK(MPI_Barrier(param->testComm),
                  "Failed to synchronize processes");

        end = MPI_Wtime();
        if (rank == 0)
                printf("Time spent inducing failure: %lf\n", (end - start));
}

static IOR_offset_t DAOS_Xfer(int access, void *file, IOR_size_t *buffer,
                              IOR_offset_t length, IOR_param_t *param)
{
        struct fileDescriptor *fd = file;
        struct aio            *aio;
        uint64_t               stripe;
        IOR_offset_t           stripeOffset;
        uint64_t               round;
        int                    rc;

        assert(length == param->transferSize);
        assert(param->offset % length == 0);

        /**
         * Currently killing only during writes
         * Kills once when 1/2 of blocksize is
         * written
         **/
        total_size += length;
        if (o.daosKill && (access == WRITE) &&
            ((param->blockSize)/2) == total_size) {
                /** More than half written lets kill */
                if (rank == 0)
                        printf("Killing and Syncing\n", rank);
                kill_and_sync(param);
                o.daosKill = 0;
        }

        /*
         * Find an available AIO descriptor.  If none, wait for one.
         */
        while (nAios == 0)
                AIOWait(param);
        aio = cfs_list_entry(aios.next, struct aio, a_list);
        cfs_list_move_tail(&aio->a_list, &aios);
        nAios--;

        stripe = (param->offset / o.daosStripeSize) %
                 o.daosStripeCount;
        rc = snprintf(aio->a_dkeyBuf, sizeof aio->a_dkeyBuf, "%lu", stripe);
        assert(rc < sizeof aio->a_dkeyBuf);
        aio->a_dkey.iov_len = strlen(aio->a_dkeyBuf) + 1;
        round = param->offset / (o.daosStripeSize * o.daosStripeCount);
        stripeOffset = o.daosStripeSize * round +
                       param->offset % o.daosStripeSize;
        if (o.daosStripeMax != 0)
                stripeOffset %= o.daosStripeMax;
        aio->a_recx.rx_idx = stripeOffset / o.daosRecordSize;
        aio->a_epochRange.epr_lo = fd->epoch;

        /*
         * If the data written will be checked later, we have to copy in valid
         * data instead of writing random bytes.  If the data being read is for
         * checking purposes, poison the buffer first.
         */
        if (access == WRITE && param->checkWrite)
                memcpy(aio->a_iov.iov_buf, buffer, length);
        else if (access == WRITECHECK || access == READCHECK)
                memset(aio->a_iov.iov_buf, '#', length);

        INFO(VERBOSE_3, param, "Starting AIO %p (%d free %d busy): access %d "
             "dkey '%s' iod <%llu, %llu> sgl <%p, %lu>", aio, nAios,
             o.daosAios - nAios, access, (char *) aio->a_dkey.iov_buf,
             (unsigned long long) aio->a_iod.iod_recxs->rx_idx,
             (unsigned long long) aio->a_iod.iod_recxs->rx_nr,
             aio->a_sgl.sg_iovs->iov_buf,
             (unsigned long long) aio->a_sgl.sg_iovs->iov_buf_len);

        if (access == WRITE) {
                rc = daos_obj_update(fd->object, fd->epoch, &aio->a_dkey,
                                     1 /* nr */, &aio->a_iod, &aio->a_sgl,
                                     &aio->a_event);
                DCHECK(rc, "Failed to start update operation");
        } else {
                rc = daos_obj_fetch(fd->object, fd->epoch, &aio->a_dkey,
                                    1 /* nr */, &aio->a_iod, &aio->a_sgl,
                                    NULL /* maps */, &aio->a_event);
                DCHECK(rc, "Failed to start fetch operation");
        }

        /*
         * If this is a WRITECHECK or READCHECK, we are expected to fill data
         * into the buffer before returning.  Note that if this is a READ, we
         * don't have to return valid data as WriteOrRead() doesn't care.
         */
        if (access == WRITECHECK || access == READCHECK) {
                while (o.daosAios - nAios > 0)
                        AIOWait(param);
                memcpy(buffer, aio->a_sgl.sg_iovs->iov_buf, length);
        }

        return length;
}

static void DAOS_Close(void *file, IOR_param_t *param)
{
        struct fileDescriptor *fd = file;
        int                    rc;

        while (o.daosAios - nAios > 0)
                AIOWait(param);
        AIOFini(param);

        ObjectClose(fd->object);

        if (param->open == WRITE && !o.daosWriteOnly) {
                /* Wait for everybody for to complete the writes. */
                MPI_CHECK(MPI_Barrier(param->testComm),
                          "Failed to synchronize processes");

                if (rank == 0) {
                        INFO(VERBOSE_2, param, "Flushing epoch %lu", fd->epoch);

                        rc = daos_epoch_flush(fd->container, fd->epoch,
                                              NULL /* state */, NULL /* ev */);
                        DCHECK(rc, "Failed to flush epoch");

                        INFO(VERBOSE_2, param, "Committing epoch %lu",
                             fd->epoch);

                        rc = daos_epoch_commit(fd->container, fd->epoch,
                                               NULL /* state */, NULL /* ev */);
                        DCHECK(rc, "Failed to commit object write");
                }
        }

        ContainerClose(fd->container, param);

        free(fd);
}

static void DAOS_Delete(char *testFileName, IOR_param_t *param)
{
        uuid_t uuid;
        int    rc;

        INFO(VERBOSE_2, param, "Deleting container %s", testFileName);

        rc = uuid_parse(testFileName, uuid);
        DCHECK(rc, "Failed to parse 'testFile': %s", testFileName);

        rc = daos_cont_destroy(pool, uuid, 1 /* force */, NULL /* ev */);
        if (rc != -DER_NONEXIST)
                DCHECK(rc, "Failed to destroy container %s", testFileName);
}

static char* DAOS_GetVersion()
{
	static char ver[1024] = {};

	sprintf(ver, "%s", "DAOS");
	return ver;
}

static void DAOS_Fsync(void *file, IOR_param_t *param)
{
        while (o.daosAios - nAios > 0)
                AIOWait(param);
}

static IOR_offset_t DAOS_GetFileSize(IOR_param_t *test, MPI_Comm testComm,
                                     char *testFileName)
{
        /*
         * Sizes are inapplicable to containers at the moment.
         */
        return 0;
}
