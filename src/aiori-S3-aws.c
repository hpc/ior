/*
* Use one object per file chunk
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <aws/auth/credentials.h>
#include <aws/common/condition_variable.h>
#include <aws/common/mutex.h>
#include <aws/common/zero.h>
#include <aws/io/channel_bootstrap.h>
#include <aws/io/event_loop.h>
#include <aws/io/logging.h>
#include <aws/io/uri.h>
#include <aws/s3/s3_client.h>


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
  char * config_file;
  char * credential_file;
  char * host; // region + aws.
  char * bucket;

  struct aws_allocator *allocator;
  struct aws_s3_client *client;
  struct aws_credentials_provider *credentials_provider;
  struct aws_client_bootstrap *client_bootstrap;
  struct aws_logger logger;
  struct aws_mutex mutex;
  struct aws_host_resolver *resolver;
  struct aws_event_loop_group *event_loop_group;
  struct aws_condition_variable c_var;
  bool execution_completed;
  struct aws_signing_config_aws signing_config;
  enum aws_log_level log_level;  
} s3_options_t;

static option_help * S3_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  s3_options_t * o = malloc(sizeof(s3_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(s3_options_t));
  }else{
    memset(o, 0, sizeof(s3_options_t));
    o->config_file     = "s3-cfg.ini";
    o->credential_file = "s3-cred.ini";
    o->bucket          = "ior";
  }

  *init_backend_options = (aiori_mod_opt_t*) o;
  option_help h [] = {
  {0, "S3-aws.host", "The host and region followed by:port. Or specify a list of hosts separated by [ ,;] to be used in a round robin fashion by the MPI ranks.", OPTION_OPTIONAL_ARGUMENT, 's', & o->host},
  {0, "S3-aws.config_file", "The AWS configuration file.", OPTION_OPTIONAL_ARGUMENT, 's', & o->config_file},
  {0, "S3-aws.credential_file", "The file with the credentials.", OPTION_OPTIONAL_ARGUMENT, 's', & o->credential_file},
  {0, "S3-aws.host", "The host/region used for the authorization signature.", OPTION_OPTIONAL_ARGUMENT, 's', & o->host},
  {0, "S3-aws.bucket", "The bucket name.", OPTION_OPTIONAL_ARGUMENT, 's', & o->bucket},
  LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

static void def_file_name(s3_options_t * o, char * out_name, char const * path){
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
    }else{
      // encode special characters
      *out_name = 'a' + (c / 26);
      out_name++;
      *out_name = 'a' + (c % 26);
      out_name++;
    }
    path++;
  }
  *out_name = 'b';
  out_name++;
  *out_name = '\0';
}

static void def_bucket_name(s3_options_t * o, char * out_name, char const * path){
  // S3_MAX_BUCKET_NAME_SIZE
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
}

struct data_handling{
  IOR_size_t * buf;
  int64_t size;
};

#define CHECK_ERROR(p) \
if (s3status != S3StatusOK){ \
  WARNF("S3 %s:%d (path:%s) \"%s\": %s %s", __FUNCTION__, __LINE__, p,  S3_get_status_name(s3status), s3error.message, s3error.furtherDetails ? s3error.furtherDetails : ""); \
}

static char * S3_getVersion()
{
  return "1.0";
}

static void S3_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * options)
{
  // Not needed
}


static void S3_Sync(aiori_mod_opt_t * options)
{
  // Not needed
}

static int S3_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_ffree = 1;
  s3_options_t * o = (s3_options_t*) options;

  return 0;
}

typedef struct{
  char * object;
} S3_fd_t;


static aiori_fd_t *S3_Create(char *path, int iorflags, aiori_mod_opt_t * options)
{
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);

  S3_fd_t * fd = malloc(sizeof(S3_fd_t));
  fd->object = strdup(p);
  return (aiori_fd_t*) fd;
}

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



  S3_fd_t * fd = malloc(sizeof(S3_fd_t));
  fd->object = strdup(p);
  return (aiori_fd_t*) fd;
}

static IOR_offset_t S3_Xfer(int access, aiori_fd_t * afd, IOR_size_t * buffer, IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * options){
  S3_fd_t * fd = (S3_fd_t *) afd;
  struct data_handling dh = { .buf = buffer, .size = length };

  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];

  if(offset != 0){
    sprintf(p, "%s-%ld-%ld", fd->object, (long) offset, (long) length);
  }else{
    sprintf(p, "%s", fd->object);
  }

  if(access == WRITE){
  }else{
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


static void S3_Delete(char *path, aiori_mod_opt_t * options)
{
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);

}

static int S3_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_bucket_name(o, p, path);

  return 0;
}

static int S3_rmdir (const char *path, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];

  def_bucket_name(o, p, path);
  return 0;
}

static int S3_stat(const char *path, struct stat *buf, aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  char p[FILENAME_MAX];
  def_file_name(o, p, path);
  memset(buf, 0, sizeof(struct stat));

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
  if(o->host == NULL){
    WARN("The S3 hostname should be specified");
  }
  return 0;
}

static void S3_init(aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;

  /* parse list of hostnames into specific host for this rank;
  this cannot be done during S3_options() because this happens before MPI_Init()  */
  if ( o->host != NULL ) {
    const char* delimiters= " ,;";

    int num_hosts = 0;
    char* r= strtok(o->host, delimiters);
    while (r != NULL) {
        num_hosts++;
        r= strtok(NULL, delimiters);
    }

    if (num_hosts > 1) {
      
      int i= rank % num_hosts;

      /* set o->host to the i'th piece separated by '\0' */
      char* next= o->host;
      while (i > 0) {

        next= strchr(next, '\0');
        next++;
        i--;
      }
      o->host= next;
    }
  }

  struct aws_allocator *allocator = aws_default_allocator();
  aws_s3_library_init(allocator);  

  // code based upon aws-c-s3/samples/s3/main.c
  o->allocator = allocator;
  o->c_var = (struct aws_condition_variable)AWS_CONDITION_VARIABLE_INIT;
  aws_mutex_init(& o->mutex);

  /* event loop */
  o->event_loop_group = aws_event_loop_group_new_default(allocator, 0, NULL);

  /* resolver */
  struct aws_host_resolver_default_options resolver_options = {
      .el_group = o->event_loop_group,
      .max_entries = 8,
  };
  o->resolver = aws_host_resolver_new_default(allocator, &resolver_options);
  
  /* client bootstrap */
  struct aws_client_bootstrap_options bootstrap_options = {
    .event_loop_group = o->event_loop_group,
    .host_resolver = o->resolver,
  };
  o->client_bootstrap = aws_client_bootstrap_new(allocator, &bootstrap_options);
  if (o->client_bootstrap == NULL) {
      ERR("ERROR initializing client bootstrap\n");
  }
  
  struct aws_credentials_provider_profile_options cred_options;
  AWS_ZERO_STRUCT(cred_options);    
  cred_options.bootstrap = o->client_bootstrap; 
  cred_options.config_file_name_override = aws_byte_cursor_from_c_str(o->config_file);
  cred_options.credentials_file_name_override = aws_byte_cursor_from_c_str(o->credential_file);
  o->credentials_provider = aws_credentials_provider_new_profile(allocator, & cred_options);
}

static void S3_final(aiori_mod_opt_t * options){
  s3_options_t * o = (s3_options_t*) options;
  aws_s3_client_release(o->client);
  aws_credentials_provider_release(o->credentials_provider);
  aws_client_bootstrap_release(o->client_bootstrap);
  aws_host_resolver_release(o->resolver);
  aws_event_loop_group_release(o->event_loop_group);
  aws_mutex_clean_up(& o->mutex);
  aws_s3_library_clean_up();
}


ior_aiori_t S3_libS3_aws = {
        .name = "S3-aws",
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
