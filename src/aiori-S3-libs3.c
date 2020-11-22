/*
* S3 implementation using the newer libs3
* https://github.com/bji/libs3
* Use one object per file chunk
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libs3.h>

#include "ior.h"
#include "aiori.h"
#include "aiori-debug.h"
#include "utilities.h"


static aiori_xfer_hint_t * hints = NULL;

static void s3_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

/************************** O P T I O N S *****************************/
typedef struct {
  int bucket_per_file;
  char * access_key;
  char * secret_key;
  char * host;
  char * bucket_prefix;
  char * bucket_prefix_cur;
  char * locationConstraint;
  char * authRegion;

  int timeout;
  int dont_suffix;
  int s3_compatible;
  int use_ssl;
  S3BucketContext bucket_context;
  S3Protocol s3_protocol;
} s3_options_t;

static option_help * S3_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  s3_options_t * o = malloc(sizeof(s3_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(s3_options_t));
  }else{
    memset(o, 0, sizeof(s3_options_t));
  }

  *init_backend_options = (aiori_mod_opt_t*) o;
  o->bucket_prefix = "ior";

  option_help h [] = {
  {0, "S3-libs3.bucket-per-file", "Use one bucket to map one file/directory, otherwise one bucket is used to store all dirs/files.", OPTION_FLAG, 'd', & o->bucket_per_file},
  {0, "S3-libs3.bucket-name-prefix", "The prefix of the bucket(s).", OPTION_OPTIONAL_ARGUMENT, 's', & o->bucket_prefix},
  {0, "S3-libs3.dont-suffix-bucket", "By default a hash will be added to the bucket name to increase uniqueness, this disables the option.", OPTION_FLAG, 'd', & o->dont_suffix },
  {0, "S3-libs3.s3-compatible", "to be selected when using S3 compatible storage", OPTION_FLAG, 'd', & o->s3_compatible },
  {0, "S3-libs3.use-ssl", "used to specify that SSL is needed for the connection", OPTION_FLAG, 'd', & o->use_ssl },
  {0, "S3-libs3.host", "The host optionally followed by:port.", OPTION_OPTIONAL_ARGUMENT, 's', & o->host},
  {0, "S3-libs3.secret-key", "The secret key.", OPTION_OPTIONAL_ARGUMENT, 's', & o->secret_key},
  {0, "S3-libs3.access-key", "The access key.", OPTION_OPTIONAL_ARGUMENT, 's', & o->access_key},
  LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

static void def_file_name(s3_options_t * o, char * out_name, char const * path){
  if(o->bucket_per_file){
    out_name += sprintf(out_name, "%s-", o->bucket_prefix_cur);
  }
  // duplicate path except "/"
  while(*path != 0){
    char c = *path;
    if(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') )){
      *out_name = *path;
      out_name++;
    }else if(c >= 'A' && c <= 'Z'){
      *out_name = *path + ('a' - 'A');
      out_name++;
    }else if(c == '/'){
      *out_name = '_';
      out_name++;
    }
    path++;
  }
  *out_name = '-';
  out_name++;
  *out_name = '\0';
}

static void def_bucket_name(s3_options_t * o, char * out_name, char const * path){
  // S3_MAX_BUCKET_NAME_SIZE
  if(o->bucket_per_file){
    out_name += sprintf(out_name, "%s-", o->bucket_prefix_cur);
  }
  // duplicate path except "/"
  while(*path != 0){
    char c = *path;
    if(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') )){
      *out_name = *path;
      out_name++;
    }else if(c >= 'A' && c <= 'Z'){
      *out_name = *path + ('a' - 'A');
      out_name++;
    }
    path++;
  }
  *out_name = '\0';

  // S3Status S3_validate_bucket_name(const char *bucketName, S3UriStyle uriStyle);
}

struct data_handling{
  IOR_size_t * buf;
  int64_t size;
};

static S3Status s3status = S3StatusInterrupted;
static S3ErrorDetails s3error = {NULL};

static S3Status responsePropertiesCallback(const S3ResponseProperties *properties, void *callbackData){
  s3status = S3StatusOK;
  return s3status;
}

static void responseCompleteCallback(S3Status status, const S3ErrorDetails *error, void *callbackData) {
  s3status = status;
  if (error == NULL){
    s3error.message = NULL;
  }else{
    s3error = *error;
  }
  return;
}

#define CHECK_ERROR(p) \
if (s3status != S3StatusOK){ \
  EWARNF("S3 %s:%d (path:%s) \"%s\": %s %s", __FUNCTION__, __LINE__, p,  S3_get_status_name(s3status), s3error.message, s3error.furtherDetails ? s3error.furtherDetails : ""); \
}


static S3ResponseHandler responseHandler = {  &responsePropertiesCallback, &responseCompleteCallback };

static char * S3_getVersion()
{
  return "0.5";
}

static void S3_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  // Not needed
}


static void S3_Sync(aiori_mod_opt_t * options)
{
  // Not needed
}

static S3Status S3ListResponseCallback(const char *ownerId, const char *ownerDisplayName, const char *bucketName, int64_t creationDateSeconds, void *callbackData){
  uint64_t * count = (uint64_t*) callbackData;
  *count++;
  return S3StatusOK;
}

static S3ListServiceHandler listhandler = { {  &responsePropertiesCallback, &responseCompleteCallback }, & S3ListResponseCallback};

static int S3_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_ffree = 1;
  s3_options_t * o = (s3_options_t*) options;

  // use the number of bucket as files
  uint64_t buckets = 0;
  S3_list_service(o->s3_protocol, o->access_key, o->secret_key, NULL, o->host,
    o->authRegion, NULL, o->timeout, & listhandler, & buckets);
  stat->f_files = buckets;
  CHECK_ERROR(o->authRegion);

  return 0;
}

static S3Status S3multipart_handler(const char *upload_id, void *callbackData){
  *((char const**)(callbackData)) = upload_id;
  return S3StatusOK;
}

static S3MultipartInitialHandler multipart_handler = { {&responsePropertiesCallback, &responseCompleteCallback }, & S3multipart_handler};

typedef struct{
  char * object;
} S3_fd_t;

static int putObjectDataCallback(int bufferSize, char *buffer, void *callbackData){
  struct data_handling * dh = (struct data_handling *) callbackData;
  const int64_t size = dh->size > bufferSize ? bufferSize : dh->size;
  if(size == 0) return 0;
  memcpy(buffer, dh->buf, size);
  dh->buf = (IOR_size_t*) ((char*)(dh->buf) + size);
  dh->size -= size;

  return size;
}

static S3PutObjectHandler putObjectHandler = { {  &responsePropertiesCallback, &responseCompleteCallback }, & putObjectDataCallback };

static aiori_fd_t *S3_Create(char *path, int iorflags, aiori_mod_opt_t * options)
{
  char * upload_id;
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);


  if(iorflags & IOR_CREAT){
    if(o->bucket_per_file){
      S3_create_bucket(o->s3_protocol, o->access_key, o->secret_key, NULL, o->host, p, o->authRegion, S3CannedAclPrivate, o->locationConstraint, NULL, o->timeout, & responseHandler, NULL);
    }else{
      struct data_handling dh = { .buf = NULL, .size = 0 };
      S3_put_object(& o->bucket_context, p, 0, NULL, NULL, o->timeout, &putObjectHandler, & dh);
    }
    if (s3status != S3StatusOK){
      CHECK_ERROR(p);
      return NULL;
    }
  }

  S3_fd_t * fd = malloc(sizeof(S3_fd_t));
  fd->object = strdup(p);
  return (aiori_fd_t*) fd;
}


static S3Status statResponsePropertiesCallback(const S3ResponseProperties *properties, void *callbackData){
  // check the size
  struct stat *buf = (struct stat*) callbackData;
  if(buf != NULL){
    buf->st_size = properties->contentLength;
    buf->st_mtime = properties->lastModified;
  }
  s3status = S3StatusOK;
  return s3status;
}

static S3ResponseHandler statResponseHandler = {  &statResponsePropertiesCallback, &responseCompleteCallback };

static aiori_fd_t *S3_Open(char *path, int flags, aiori_mod_opt_t * options)
{
  if(flags & IOR_CREAT){
    return S3_Create(path, flags, options);
  }
  if(flags & IOR_WRONLY){
    WARN("S3 IOR_WRONLY is not supported");
  }
  if(flags & IOR_RDWR){
    WARN("S3 IOR_RDWR is not supported");
  }

  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);

  if (o->bucket_per_file){
    S3_test_bucket(o->s3_protocol, S3UriStylePath, o->access_key, o->secret_key,
                        NULL, o->host, p, o->authRegion, 0, NULL,
                        NULL, o->timeout, & responseHandler, NULL);
  }else{
    struct stat buf;
    S3_head_object(& o->bucket_context, p, NULL, o->timeout, & statResponseHandler, & buf);
  }
  if (s3status != S3StatusOK){
    CHECK_ERROR(p);
    return NULL;
  }

  S3_fd_t * fd = malloc(sizeof(S3_fd_t));
  fd->object = strdup(p);
  return (aiori_fd_t*) fd;
}

static S3Status getObjectDataCallback(int bufferSize, const char *buffer,  void *callbackData){
  struct data_handling * dh = (struct data_handling *) callbackData;
  const int64_t size = dh->size > bufferSize ? bufferSize : dh->size;
  memcpy(dh->buf, buffer, size);
  dh->buf = (IOR_size_t*) ((char*)(dh->buf) + size);
  dh->size -= size;

  return S3StatusOK;
}

static S3GetObjectHandler getObjectHandler = { {  &responsePropertiesCallback, &responseCompleteCallback }, & getObjectDataCallback };

static IOR_offset_t S3_Xfer(int access, aiori_fd_t * afd, IOR_size_t * buffer, IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * options){
  S3_fd_t * fd = (S3_fd_t *) afd;
  struct data_handling dh = { .buf = buffer, .size = length };

  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];

  if(o->bucket_per_file){
    o->bucket_context.bucketName = fd->object;
    if(offset != 0){
      sprintf(p, "%ld-%ld", (long) offset, (long) length);
    }else{
      sprintf(p, "0");
    }
  }else{
    if(offset != 0){
      sprintf(p, "%s-%ld-%ld", fd->object, (long) offset, (long) length);
    }else{
      sprintf(p, "%s", fd->object);
    }
  }
  if(access == WRITE){
    S3_put_object(& o->bucket_context, p, length, NULL, NULL, o->timeout, &putObjectHandler, & dh);
  }else{
    S3_get_object(& o->bucket_context, p, NULL, 0, length, NULL, o->timeout, &getObjectHandler, & dh);
  }
  if (! o->s3_compatible){
    CHECK_ERROR(p);
  }
  return length;
}


static void S3_Close(aiori_fd_t * afd, aiori_mod_opt_t * options)
{
  S3_fd_t * fd = (S3_fd_t *) afd;
  free(fd->object);
  free(afd);
}

typedef struct {
  int status; // do not reorder!
  s3_options_t * o;
  int truncated;
  char const *nextMarker;
} s3_delete_req;

S3Status list_delete_cb(int isTruncated, const char *nextMarker, int contentsCount, const S3ListBucketContent *contents, int commonPrefixesCount, const char **commonPrefixes, void *callbackData){
  s3_delete_req * req = (s3_delete_req*) callbackData;
  for(int i=0; i < contentsCount; i++){
    S3_delete_object(& req->o->bucket_context, contents[i].key, NULL, req->o->timeout, & responseHandler, NULL);
  }
  req->truncated = isTruncated;
  if(isTruncated){
    req->nextMarker = nextMarker;
  }
  return S3StatusOK;
}

static S3ListBucketHandler list_delete_handler = {{&responsePropertiesCallback, &responseCompleteCallback }, list_delete_cb};

static void S3_Delete(char *path, aiori_mod_opt_t * options)
{
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);


  if(o->bucket_per_file){
    o->bucket_context.bucketName = p;
    s3_delete_req req = {0, o, 0, NULL};
    do{
      S3_list_bucket(& o->bucket_context, NULL, req.nextMarker, NULL, INT_MAX, NULL, o->timeout, & list_delete_handler, & req);
    }while(req.truncated);
    S3_delete_bucket(o->s3_protocol, S3UriStylePath, o->access_key, o->secret_key, NULL, o->host, p, o->authRegion, NULL,  o->timeout, & responseHandler, NULL);
  }else{
    s3_delete_req req = {0, o, 0, NULL};
    do{
      S3_list_bucket(& o->bucket_context, p, req.nextMarker, NULL, INT_MAX, NULL, o->timeout, & list_delete_handler, & req);
    }while(req.truncated);
    S3_delete_object(& o->bucket_context, p, NULL, o->timeout, & responseHandler, NULL);
  }
  CHECK_ERROR(p);
}

static int S3_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_bucket_name(o, p, path);


  if (o->bucket_per_file){
    S3_create_bucket(o->s3_protocol, o->access_key, o->secret_key, NULL, o->host, p, o->authRegion, S3CannedAclPrivate, o->locationConstraint, NULL, o->timeout, & responseHandler, NULL);
    CHECK_ERROR(p);
    return 0;
  }else{
    struct data_handling dh = { .buf = NULL, .size = 0 };
    S3_put_object(& o->bucket_context, p, 0, NULL, NULL, o->timeout, & putObjectHandler, & dh);
    if (! o->s3_compatible){
      CHECK_ERROR(p);
    }
    return 0;
  }
}

static int S3_rmdir (const char *path, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];

  def_bucket_name(o, p, path);
  if (o->bucket_per_file){
    S3_delete_bucket(o->s3_protocol, S3UriStylePath, o->access_key, o->secret_key, NULL, o->host, p, o->authRegion, NULL,  o->timeout, & responseHandler, NULL);
    CHECK_ERROR(p);
    return 0;
  }else{
    S3_delete_object(& o->bucket_context, p, NULL, o->timeout, & responseHandler, NULL);
    CHECK_ERROR(p);
    return 0;
  }
}

static int S3_stat(const char *path, struct stat *buf, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);
  memset(buf, 0, sizeof(struct stat));
  // TODO count the individual file fragment sizes together
  if (o->bucket_per_file){
    S3_test_bucket(o->s3_protocol, S3UriStylePath, o->access_key, o->secret_key,
                        NULL, o->host, p, o->authRegion, 0, NULL,
                        NULL, o->timeout, & responseHandler, NULL);
  }else{
    S3_head_object(& o->bucket_context, p, NULL, o->timeout, & statResponseHandler, buf);
  }
  if (s3status != S3StatusOK){
    return -1;
  }
  return 0;
}

static int S3_access (const char *path, int mode, aiori_mod_opt_t * options){
  struct stat buf;
  return S3_stat(path, & buf, options);
}

static IOR_offset_t S3_GetFileSize(aiori_mod_opt_t * options, char *testFileName)
{
  struct stat buf;
  if(S3_stat(testFileName, & buf, options) != 0) return -1;
  return buf.st_size;
}


static int S3_check_params(aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  if(o->access_key == NULL){
    o->access_key = "";
  }
  if(o->secret_key == NULL){
    o->secret_key = "";
  }
  if(o->host == NULL){
    WARN("The S3 hostname should be specified");
  }
  return 0;
}

static void S3_init(aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  int ret = S3_initialize(NULL, S3_INIT_ALL, o->host);
  if(ret != S3StatusOK)
    FAIL("Could not initialize S3 library");

  // create a bucket id based on access-key using a trivial checksumming
  if(! o->dont_suffix){
    uint64_t c = 0;
    char * r = o->access_key;
    for(uint64_t pos = 1; (*r) != '\0' ; r++, pos*=10) {
      c += (*r) * pos;
    }
    int count = snprintf(NULL, 0, "%s%lu", o->bucket_prefix, c % 1000);
    char * old_prefix = o->bucket_prefix;
    o->bucket_prefix_cur = malloc(count + 1);
    sprintf(o->bucket_prefix_cur, "%s%lu", old_prefix, c % 1000);
  }else{
    o->bucket_prefix_cur = o->bucket_prefix;
  }

  // init bucket context
  memset(& o->bucket_context, 0, sizeof(o->bucket_context));
  o->bucket_context.hostName = o->host;
  o->bucket_context.bucketName = o->bucket_prefix_cur;
  if (o->use_ssl){
    o->s3_protocol = S3ProtocolHTTPS;
  }else{
    o->s3_protocol  = S3ProtocolHTTP;
  }
  o->bucket_context.protocol = o->s3_protocol;
  o->bucket_context.uriStyle = S3UriStylePath;
  o->bucket_context.accessKeyId = o->access_key;
  o->bucket_context.secretAccessKey = o->secret_key;

  if (! o->bucket_per_file && rank == 0){
    S3_create_bucket(o->s3_protocol, o->access_key, o->secret_key, NULL, o->host, o->bucket_context.bucketName, o->authRegion, S3CannedAclPrivate, o->locationConstraint, NULL, o->timeout, & responseHandler, NULL);
    CHECK_ERROR(o->bucket_context.bucketName);
  }

  if ( ret != S3StatusOK ){
    FAIL("S3 error %s", S3_get_status_name(ret));
  }
}

static void S3_final(aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  if (! o->bucket_per_file && rank == 0){
    S3_delete_bucket(o->s3_protocol, S3UriStylePath, o->access_key, o->secret_key, NULL, o->host,  o->bucket_context.bucketName, o->authRegion, NULL,  o->timeout, & responseHandler, NULL);
    CHECK_ERROR(o->bucket_context.bucketName);
  }

  S3_deinitialize();
}


ior_aiori_t S3_libS3_aiori = {
        .name = "S3-libs3",
        .name_legacy = NULL,
        .create = S3_Create,
        .open = S3_Open,
        .xfer = S3_Xfer,
        .close = S3_Close,
        .delete = S3_Delete,
        .get_version = S3_getVersion,
        .fsync = S3_Fsync,
        .xfer_hints = s3_xfer_hints,
        .get_file_size = S3_GetFileSize,
        .statfs = S3_statfs,
        .mkdir = S3_mkdir,
        .rmdir = S3_rmdir,
        .access = S3_access,
        .stat = S3_stat,
        .initialize = S3_init,
        .finalize = S3_final,
        .get_options = S3_options,
        .check_params = S3_check_params,
        .sync = S3_Sync,
        .enable_mdtest = true
};
