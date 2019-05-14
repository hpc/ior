#ifndef _IOR_OPTION_H
#define _IOR_OPTION_H

#include <stdint.h>

/*
 * Initial version by JK
 */

typedef enum{
  OPTION_FLAG,
  OPTION_OPTIONAL_ARGUMENT,
  OPTION_REQUIRED_ARGUMENT
} option_value_type;

typedef struct{
  char shortVar;
  char * longVar;
  char * help;

  option_value_type arg;
  char type;  // data type, H = hidden string
  void * variable;
} option_help;

typedef struct{
  char * prefix; // may be NULL to include it in the standard name
  option_help * options;
  void * defaults; // these default values are taken from the command line
} option_module;

typedef struct{
  int module_count;
  option_module * modules;
} options_all_t;

#define LAST_OPTION {0, 0, 0, (option_value_type) 0, 0, NULL}

int64_t string_to_bytes(char *size_str);
void option_print_current(option_help * args);

//@return the number of parsed arguments
int option_parse(int argc, char ** argv, options_all_t * args);

/* Parse a single line */
int option_parse_key_value(char * key, char * value, options_all_t * opt_all);

#endif
