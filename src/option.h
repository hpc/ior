#ifndef _IOR_OPTION_H
#define _IOR_OPTION_H

#include <stdint.h>

/*
 * Initial revision by JK
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

#define LAST_OPTION {0, 0, 0, (option_value_type) 0, 0, NULL}

int64_t string_to_bytes(char *size_str);
void option_print_help(option_help * args, int is_plugin);
void option_print_current(option_help * args);

//@return the number of parsed arguments
int option_parse(int argc, char ** argv, option_help * args, int * print_help);

#endif
