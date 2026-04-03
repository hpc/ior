#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

#include "ior.h"
#include "aiori.h"
#include "aiori-debug.h"
#include "utilities.h"


static aiori_xfer_hint_t * hints = NULL;

static void s3_curl_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

typedef struct {
  char* access_key;
  char* secret_key;
  char* host;
  char* bucket_name;
  char* region;
  int use_ssl;
  int timeout;
  int verify_ssl;
} s3_curl_options_t;

typedef struct {
  char* key;
  CURL* curl;
  int is_open;
} s3_curl_fd_t;

typedef struct {
  char* memory;
  size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static size_t ReadDataCallback(void *ptr, size_t size, size_t nmemb, void *userp) {
  MemoryStruct *data = (MemoryStruct *)userp;
  size_t max_len = size * nmemb;
  size_t copy_len = (data->size < max_len) ? data->size : max_len;
  
  if (copy_len > 0) {
    memcpy(ptr, data->memory, copy_len);
    data->size -= copy_len;
    data->memory += copy_len;
  }
  
  return copy_len;
}

static int S3_curl_check_params(aiori_mod_opt_t * options){
  s3_curl_options_t * o = (s3_curl_options_t*) options;
  if(o->access_key == NULL){
    ERR("S3-curl: access-key not specified\n");
    return 1;
  }
  if(o->secret_key == NULL){
    ERR("S3-curl: secret-key not specified\n");
    return 1;
  }
  if(o->bucket_name == NULL){
    ERR("S3-curl: bucket not specified\n");
    return 1;
  }
  return 0;
}

static option_help * S3_curl_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  s3_curl_options_t * o = malloc(sizeof(s3_curl_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(s3_curl_options_t));
  }else{
    memset(o, 0, sizeof(s3_curl_options_t));
  }

  *init_backend_options = (aiori_mod_opt_t*) o;
  o->bucket_name = "test-bucket";
  o->region = "us-east-1";
  o->use_ssl = 0;
  o->timeout = 5;
  o->verify_ssl = 0;

  option_help h [] = {
    {0, "S3-curl.bucket", "Name of the S3 bucket", OPTION_OPTIONAL_ARGUMENT, 's', & o->bucket_name},
    {0, "S3-curl.access-key", "AWS Access Key ID", OPTION_OPTIONAL_ARGUMENT, 's', & o->access_key},
    {0, "S3-curl.secret-key", "AWS Secret Access Key", OPTION_OPTIONAL_ARGUMENT, 's', & o->secret_key},
    {0, "S3-curl.host", "S3 endpoint hostname", OPTION_OPTIONAL_ARGUMENT, 's', & o->host},
    {0, "S3-curl.region", "AWS region", OPTION_OPTIONAL_ARGUMENT, 's', & o->region},
    {0, "S3-curl.use-ssl", "Use HTTPS", OPTION_FLAG, 'd', & o->use_ssl},
    {0, "S3-curl.timeout", "Timeout in seconds", OPTION_OPTIONAL_ARGUMENT, 'd', & o->timeout},
    {0, "S3-curl.verify-ssl", "Verify SSL certificates", OPTION_FLAG, 'd', & o->verify_ssl},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

static CURL* init_curl_handle(s3_curl_options_t* options) {
  CURL* curl = curl_easy_init();
  if(!curl) {
    return NULL;
  }

  char host[1024];
  if(options->host != NULL) {
    snprintf(host, sizeof(host), "%s", options->host);
  } else {
    snprintf(host, sizeof(host), "s3.%s.amazonaws.com", options->region);
  }

  curl_easy_setopt(curl, CURLOPT_USERNAME, options->access_key);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, options->secret_key);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, options->timeout);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if(!options->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  return curl;
}

static void set_s3_url(CURL* curl, s3_curl_options_t* options, const char* key, const char* method) {
  char host[512];
  if(options->host != NULL) {
    snprintf(host, sizeof(host), "%s", options->host);
  } else {
    snprintf(host, sizeof(host), "s3.%s.amazonaws.com", options->region);
  }

  char url[2048];
  if(options->use_ssl) {
    snprintf(url, sizeof(url), "https://%s/%s/%s", host, options->bucket_name, key);
  } else {
    snprintf(url, sizeof(url), "http://%s/%s/%s", host, options->bucket_name, key);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
}

static void S3_curl_initialize(aiori_mod_opt_t * options){
  curl_global_init(CURL_GLOBAL_ALL);
}

static void S3_curl_finalize(aiori_mod_opt_t * options){
  curl_global_cleanup();
}

static aiori_fd_t *S3_curl_Create(char *testFileName, int iorflags, aiori_mod_opt_t * options){
  if(S3_curl_check_params(options) != 0){
    return NULL;
  }

  s3_curl_options_t * o = (s3_curl_options_t*) options;
  s3_curl_fd_t * fd = malloc(sizeof(s3_curl_fd_t));

  fd->key = strdup(testFileName);
  fd->curl = init_curl_handle(o);
  if(!fd->curl) {
    free(fd->key);
    free(fd);
    return NULL;
  }
  fd->is_open = 1;

  set_s3_url(fd->curl, o, testFileName, "PUT");

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
  headers = curl_slist_append(headers, "x-amz-content-sha256: UNSIGNED-PAYLOAD");
  curl_easy_setopt(fd->curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(fd->curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(fd->curl, CURLOPT_POSTFIELDSIZE, 0L);
  curl_easy_setopt(fd->curl, CURLOPT_INFILESIZE, 0L);

  CURLcode res = curl_easy_perform(fd->curl);
  
  long http_code = 0;
  curl_easy_getinfo(fd->curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);

  if(res != CURLE_OK || http_code >= 400) {
    if(verbose > 0) {
      fprintf(out_logfile, "S3-curl create failed: %s (HTTP %ld)\n", 
              curl_easy_strerror(res), http_code);
    }
    curl_easy_cleanup(fd->curl);
    free(fd->key);
    free(fd);
    return NULL;
  }

  return (aiori_fd_t*) fd;
}

static aiori_fd_t *S3_curl_Open(char *testFileName, int flags, aiori_mod_opt_t * options){
  if(S3_curl_check_params(options) != 0){
    return NULL;
  }

  s3_curl_options_t * o = (s3_curl_options_t*) options;
  s3_curl_fd_t * fd = malloc(sizeof(s3_curl_fd_t));

  fd->key = strdup(testFileName);
  fd->curl = init_curl_handle(o);
  if(!fd->curl) {
    free(fd->key);
    free(fd);
    return NULL;
  }
  fd->is_open = 1;

  return (aiori_fd_t*) fd;
}

static void S3_curl_Close(aiori_fd_t *fd, aiori_mod_opt_t * options){
  if(fd == NULL){
    return;
  }

  s3_curl_fd_t * s3fd = (s3_curl_fd_t*) fd;

  if(s3fd->curl != NULL) {
    curl_easy_cleanup(s3fd->curl);
  }
  
  if(s3fd->key != NULL){
    free(s3fd->key);
  }
  free(s3fd);
}

static void S3_curl_Delete(char *testFileName, aiori_mod_opt_t * options){
  if(S3_curl_check_params(options) != 0){
    return;
  }

  s3_curl_options_t * o = (s3_curl_options_t*) options;

  CURL* curl = init_curl_handle(o);
  if(!curl) {
    return;
  }

  set_s3_url(curl, o, testFileName, "DELETE");

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
}

static IOR_offset_t S3_curl_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer,
                                  IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * options){
  if(file == NULL || S3_curl_check_params(options) != 0){
    return -1;
  }

  s3_curl_fd_t * s3fd = (s3_curl_fd_t*) file;
  s3_curl_options_t * o = (s3_curl_options_t*) options;

  IOR_offset_t bytes_transferred = 0;

  if(access == WRITE) {
    set_s3_url(s3fd->curl, o, s3fd->key, "PUT");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "x-amz-content-sha256: UNSIGNED-PAYLOAD");
    curl_easy_setopt(s3fd->curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(s3fd->curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(s3fd->curl, CURLOPT_POSTFIELDS, buffer);
    curl_easy_setopt(s3fd->curl, CURLOPT_POSTFIELDSIZE, (long)length);

    CURLcode res = curl_easy_perform(s3fd->curl);
    
    long http_code = 0;
    curl_easy_getinfo(s3fd->curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);

    if(res != CURLE_OK || http_code >= 400) {
      if(verbose > 0) {
        fprintf(out_logfile, "S3-curl write failed: %s (HTTP %ld)\n", 
                curl_easy_strerror(res), http_code);
      }
      return -1;
    }

    bytes_transferred = length;
  } else {
    set_s3_url(s3fd->curl, o, s3fd->key, "GET");

    MemoryStruct response;
    response.memory = (char*)buffer;
    response.size = length;

    curl_easy_setopt(s3fd->curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(s3fd->curl, CURLOPT_WRITEDATA, (void*)&response);

    CURLcode res = curl_easy_perform(s3fd->curl);
    
    long http_code = 0;
    curl_easy_getinfo(s3fd->curl, CURLINFO_RESPONSE_CODE, &http_code);

    if(res != CURLE_OK || http_code >= 400) {
      if(verbose > 0) {
        fprintf(out_logfile, "S3-curl read failed: %s (HTTP %ld)\n", 
                curl_easy_strerror(res), http_code);
      }
      return -1;
    }

    bytes_transferred = response.size;
  }

  return bytes_transferred;
}

static void S3_curl_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * options){
  return;
}

static char * S3_curl_getVersion(){
  return "1.0";
}

static IOR_offset_t S3_curl_GetFileSize(aiori_mod_opt_t * options, char *testFileName){
  if(S3_curl_check_params(options) != 0){
    return 0;
  }

  s3_curl_options_t * o = (s3_curl_options_t*) options;

  CURL* curl = init_curl_handle(o);
  if(!curl) {
    return 0;
  }

  set_s3_url(curl, o, testFileName, "HEAD");

  CURLcode res = curl_easy_perform(curl);
  
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  if(res != CURLE_OK || http_code >= 400) {
    curl_easy_cleanup(curl);
    return 0;
  }

  curl_off_t cl;
  res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);

  curl_easy_cleanup(curl);

  if(res != CURLE_OK) {
    return 0;
  }

  return (IOR_offset_t)cl;
}

static int S3_curl_statfs (const char * path, ior_aiori_statfs_t * stat, aiori_mod_opt_t * options){
  if(stat == NULL){
    return -1;
  }
  stat->f_bsize = 1;
  stat->f_blocks = 1;
  stat->f_bfree = 1;
  stat->f_bavail = 1;
  stat->f_files = 1;
  stat->f_ffree = 1;
  return 0;
}

static int S3_curl_mkdir (const char *path, mode_t mode, aiori_mod_opt_t * options){
  return 0;
}

static int S3_curl_rmdir (const char *path, aiori_mod_opt_t * options){
  return 0;
}

static int S3_curl_access (const char *path, int mode, aiori_mod_opt_t * options){
  if(S3_curl_check_params(options) != 0){
    return -1;
  }

  s3_curl_options_t * o = (s3_curl_options_t*) options;

  CURL* curl = init_curl_handle(o);
  if(!curl) {
    return -1;
  }

  set_s3_url(curl, o, path, "HEAD");

  CURLcode res = curl_easy_perform(curl);
  
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_easy_cleanup(curl);

  if(res == CURLE_OK && http_code == 200) {
    return 0;
  }
  return -1;
}

static int S3_curl_stat (const char *path, struct stat *buf, aiori_mod_opt_t * options){
  if(buf == NULL){
    return -1;
  }

  IOR_offset_t size = S3_curl_GetFileSize(options, (char*)path);
  if(size == 0) {
    return -1;
  }

  memset(buf, 0, sizeof(struct stat));
  buf->st_size = size;
  buf->st_mode = S_IFREG | 0644;

  return 0;
}

static int S3_curl_rename (const char *oldpath, const char *newpath, aiori_mod_opt_t * options){
  return -1;
}

static void S3_curl_sync(aiori_mod_opt_t * options){
  return;
}

ior_aiori_t s3_curl_aiori = {
  .name = "S3-curl",
  .name_legacy = NULL,
  .create = S3_curl_Create,
  .open = S3_curl_Open,
  .xfer = S3_curl_Xfer,
  .close = S3_curl_Close,
  .remove = S3_curl_Delete,
  .get_version = S3_curl_getVersion,
  .fsync = S3_curl_Fsync,
  .get_file_size = S3_curl_GetFileSize,
  .statfs = S3_curl_statfs,
  .mkdir = S3_curl_mkdir,
  .rmdir = S3_curl_rmdir,
  .rename = S3_curl_rename,
  .access = S3_curl_access,
  .stat = S3_curl_stat,
  .initialize = S3_curl_initialize,
  .finalize = S3_curl_finalize,
  .get_options = S3_curl_options,
  .check_params = S3_curl_check_params,
  .sync = S3_curl_sync,
  .enable_mdtest = true
};