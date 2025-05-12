#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include <option.h>


/* merge two option lists and return the total size */
option_help * option_merge(option_help * a, option_help * b){
  int count_a = 0;
  for(option_help * i = a; i->type != 0; i++){
    count_a++;
  }
  int count = count_a + 1; // LAST_OPTION is one
  for(option_help * i = b; i->type != 0; i++){
    count++;
  }
  option_help * h = malloc(sizeof(option_help) * count);
  memcpy(h, a, sizeof(option_help) * count_a);
  memcpy(h + count_a, b, sizeof(option_help) * (count - count_a));
  return h;
}

/*
* Takes a string of the form 64, 8m, 128k, 4g, etc. and converts to bytes.
*/
int64_t string_to_bytes(char *size_str)
{
       int64_t size = 0;
       char range;
       int rc;

       rc = sscanf(size_str, " %lld %c ", (long long*) & size, &range);
       if (rc == 2) {
               switch ((int)range) {
               case 'k':
               case 'K':
                       size <<= 10;
                       break;
               case 'm':
               case 'M':
                       size <<= 20;
                       break;
               case 'g':
               case 'G':
                       size <<= 30;
                       break;
               case 't':
               case 'T':
                       size <<= 40;
                       break;
               case 'p':
               case 'P':
                       size <<= 50;
                       break;
               }
       } else if (rc == 0) {
               size = -1;
       }
       return (size);
}

/*
 * Initial revision by JK
 */

static int print_value(option_help * o){
  int pos = 0;
  if (o->arg == OPTION_OPTIONAL_ARGUMENT || o->arg == OPTION_REQUIRED_ARGUMENT){
    assert(o->variable != NULL);

    switch(o->type){
      case('p'):{
        pos += printf("=STRING");
        break;
      }
      case('F'):{
        pos += printf("=%.14f ", *(double*) o->variable);
        break;
      }
      case('f'):{
        pos += printf("=%.6f ", (double) *(float*) o->variable);
        break;
      }
      case('d'):{
        pos += printf("=%d ", *(int*) o->variable);
        break;
      }
      case('H'):
      case('s'):{
        if ( *(char**) o->variable != NULL &&  ((char**) o->variable)[0][0] != 0 ){
          pos += printf("=%s", *(char**) o->variable);
        }else{
          pos += printf("=STRING");
        }
        break;
      }
      case('c'):{
        pos += printf("=%c", *(char*) o->variable);
        break;
      }
      case('l'):{
        pos += printf("=%lld", *(long long*) o->variable);
        break;
      }
      case('u'):{
        pos += printf("=%lu", *(uint64_t*) o->variable);
        break;
      }
    }
  }
  if (o->arg == OPTION_FLAG && (*(int*)o->variable) != 0){
    pos += printf(" (%d)", (*(int*)o->variable));
  }

  return pos;
}

static void print_help_section(option_help * args, option_value_type type, char * name){
  int first;
  first = 1;
  option_help * o;
  for(o = args; o->shortVar != 0 || o->longVar != 0 || o->help != NULL ; o++){

    if (o->arg == type){
      if( o->shortVar == 0 && o->longVar == 0 && o->help != NULL){
        printf("%s\n", o->help);
        continue;
      }
      if (first){
        printf("\n%s\n", name);
        first = 0;
      }
      printf("  ");
      int pos = 0;
      if(o->shortVar != 0 && o->longVar != 0){
        pos += printf("-%c, --%s", o->shortVar, o->longVar);
      }else if(o->shortVar != 0){
        pos += printf("-%c", o->shortVar);
      }else if(o->longVar != 0){
        pos += printf("--%s", o->longVar);
      }

      pos += print_value(o);
      if(o->help != NULL){
        for(int i = 0 ; i < (30 - pos); i++){
          printf(" ");
        }
        printf("%s", o->help);
      }
      printf("\n");
    }
  }
}

void option_print_help(option_help * args){
  print_help_section(args, OPTION_REQUIRED_ARGUMENT, "Required arguments");
  print_help_section(args, OPTION_FLAG, "Flags");
  print_help_section(args, OPTION_OPTIONAL_ARGUMENT, "Optional arguments");
}


static int print_option_value(option_help * o){
  int pos = 0;
  if (o->arg == OPTION_OPTIONAL_ARGUMENT || o->arg == OPTION_REQUIRED_ARGUMENT){
    assert(o->variable != NULL);

    switch(o->type){
      case('F'):{
        pos += printf("=%.14f ", *(double*) o->variable);
        break;
      }
      case('f'):{
        pos += printf("=%.6f ", (double) *(float*) o->variable);
        break;
      }
      case('d'):{
        pos += printf("=%d ", *(int*) o->variable);
        break;
      }
      case('H'):{
        pos += printf("=HIDDEN");
        break;
      }
      case('s'):{
        if ( *(char**) o->variable != NULL &&  ((char**) o->variable)[0][0] != 0 ){
          pos += printf("=%s", *(char**) o->variable);
        }else{
          pos += printf("=");
        }
        break;
      }
      case('c'):{
        pos += printf("=%c", *(char*) o->variable);
        break;
      }
      case('l'):{
        pos += printf("=%lld", *(long long*) o->variable);
        break;
      }
      case('u'):{
        pos += printf("=%lu", *(uint64_t*) o->variable);
        break;
      }
    }
  }else{
    //printf(" ");
  }

  return pos;
}


static void print_current_option_section(option_help * args, option_value_type type){
  option_help * o;
  for(o = args; o->shortVar != 0 || o->longVar != 0 ; o++){
    if (o->arg == type){
      int pos = 0;
      if (o->arg == OPTION_FLAG && (*(int*)o->variable) == 0){
        continue;
      }
      printf("\t");

      if(o->shortVar != 0 && o->longVar != 0){
        pos += printf("%s", o->longVar);
      }else if(o->shortVar != 0){
        pos += printf("%c", o->shortVar);
      }else if(o->longVar != 0){
        pos += printf("%s", o->longVar);
      }

      pos += print_option_value(o);
      printf("\n");
    }
  }
}


void option_print_current(option_help * args){
  print_current_option_section(args, OPTION_REQUIRED_ARGUMENT);
  print_current_option_section(args, OPTION_OPTIONAL_ARGUMENT);
  print_current_option_section(args, OPTION_FLAG);
}

static void option_parse_token(char ** argv, int * flag_parsed_next, int * requiredArgsSeen, options_all_t * opt_all, int * error, int * print_help){
  char * txt = argv[0];
  char * arg = strstr(txt, "=");

  int replaced_equal = 0;
  int i = 0;
  if(arg != NULL){
    arg[0] = 0;
    replaced_equal = 1;

    // Check empty value
    arg = (arg[1] == 0) ? NULL : arg + 1;
  }
  *flag_parsed_next = 0;

  // just skip over the first dash so we don't have to handle it everywhere below
  if(txt[0] != '-'){
      *error = 1;
      return;
  }
  txt++;
  int parsed = 0;
  
  // printf("Parsing: %s : %s\n", txt, arg);
  // support groups of multiple flags like -vvv or -vq
  for(int flag_index = 0; flag_index < strlen(txt); ++flag_index){
    // don't loop looking for multiple flags if we already processed a long option
    if(txt[flag_index] == '=' || (txt[0] == '-' && flag_index > 0))
        break;

    for(int m = 0; m < opt_all->module_count; m++ ){
      option_help * args = opt_all->modules[m].options;
      if(args == NULL) continue;
      // try to find matching option help
      for(option_help * o = args; o->shortVar != 0 || o->longVar != 0 || o->help != NULL ; o++ ){
        if( o->shortVar == 0 && o->longVar == 0 ){
          // section
          continue;
        }
        if ( (o->shortVar == txt[flag_index]) || (strlen(txt) > 2 && txt[0] == '-' && o->longVar != NULL && strcmp(txt + 1, o->longVar) == 0)){
          //  printf("Found %s %c=%c? %d %d\n", o->help, o->shortVar, txt[flag_index], (o->shortVar == txt[flag_index]), (strlen(txt) > 2 && txt[0] == '-' && o->longVar != NULL && strcmp(txt + 1, o->longVar) == 0));
          // now process the option.
          switch(o->arg){
            case (OPTION_FLAG):{
              assert(o->type == 'd');
              if(arg != NULL){
                int val = atoi(arg);
                (*(int*) o->variable) = (val < 0) ? 0 : val;
              }else{
                (*(int*) o->variable)++;
              }
              break;
            }
            case (OPTION_OPTIONAL_ARGUMENT):
            case (OPTION_REQUIRED_ARGUMENT):{
              // check if next is an argument
              if(arg == NULL && replaced_equal != 1){
                if(o->shortVar == txt[0] && txt[1] != 0){
                  arg = & txt[1];
                }else{
                  // simply take the next value as argument
                  i++;
                  arg = argv[1];
                  *flag_parsed_next = 1;
                }
              }

              if(arg == NULL){
                const char str[] = {o->shortVar, 0};
                printf("Error, argument missing for option %s\n", (o->longVar != NULL) ? o->longVar : str);
                exit(EXIT_FAILURE);
              }

              switch(o->type){
                case('p'):{
                  // call the function in the variable
                  void(*fp)(void*) = o->variable;
                  fp(arg);
                  break;
                }
                case('F'):{
                  *(double*) o->variable = atof(arg);
                  break;
                }
                case('f'):{
                  *(float*) o->variable = atof(arg);
                  break;
                }
                case('d'):{
                  int64_t val = string_to_bytes(arg);
                  if (val > INT_MAX || val < INT_MIN){
                    printf("WARNING: parsing the number %s to integer, this produced an overflow!\n", arg);
                  }
                  *(int*) o->variable = val;
                  break;
                }
                case('H'):
                case('s'):{
                  (*(char **) o->variable) = strdup(arg);
                  break;
                }
                case('c'):{
                  (*(char *)o->variable) = arg[0];
                  if(strlen(arg) > 1){
                    printf("Error, ignoring remainder of string for option %c (%s).\n", o->shortVar, o->longVar);
                  }
                  break;
                }
                case('l'):{
                  *(long long*) o->variable = string_to_bytes(arg);
                  break;
                }
                case('u'):{
                  *(uint64_t*) o->variable = string_to_bytes(arg);
                  break;
                }
                default:
                  printf("ERROR: Unknown option type %c\n", o->type);
                  break;
              }
            }
          }
          if(replaced_equal){
            arg[-1] = '=';
          }

          if(o->arg == OPTION_REQUIRED_ARGUMENT){
            (*requiredArgsSeen)++;
          }

          parsed = 1;
        }
      }
    }
  }
  if(parsed) return;
  
  if(strcmp(txt, "h") == 0 || strcmp(txt, "-help") == 0){
    *print_help = 1;
  }else{
    *error = 1;
  }
}

int option_parse_str(char*val, options_all_t * opt_all){
    int flag_parsed_next;
    int error = 0;
    int requiredArgsSeen = 0;
    int print_help = 0;
    char * argv[2] = {val, NULL};
    option_parse_token(argv, & flag_parsed_next, & requiredArgsSeen, opt_all, & error, & print_help);
    return error;
}

int option_parse_key_value(char * key, char *val, options_all_t * opt_all){
  int flag_parsed_next;
  int error = 0;
  int requiredArgsSeen = 0;
  int print_help = 0;
  char value[1024];
  sprintf(value, "%s=%s", key, val);
  char * argv[2] = {value, NULL};
  option_parse_token(argv, & flag_parsed_next, & requiredArgsSeen, opt_all, & error, & print_help);
  return error;
}

int option_parse(int argc, char ** argv, options_all_t * opt_all){
  int error = 0;
  int requiredArgsSeen = 0;
  int requiredArgsNeeded = 0;
  int i;
  int printhelp = 0;

  for(int m = 0; m < opt_all->module_count; m++ ){
    option_help * args = opt_all->modules[m].options;
    if(args == NULL) continue;
    for(option_help * o = args; o->shortVar != 0 || o->longVar != 0 ; o++ ){
      if(o->arg == OPTION_REQUIRED_ARGUMENT){
        requiredArgsNeeded++;
      }
    }
  }

  for(i=1; i < argc; i++){
    int flag_parsed_next;
    option_parse_token(& argv[i], & flag_parsed_next, & requiredArgsSeen, opt_all, & error, & printhelp);
    if (flag_parsed_next){
      i++;
    }
    if(error){
      printf("Error invalid argument: %s\n", argv[i]);
    }
  }

  if( requiredArgsSeen != requiredArgsNeeded ){
    printf("Error: Missing some required arguments\n\n");
    printhelp = 1;
  }

  if(error != 0){
    printf("Invalid options\n");
    printhelp = 1;
  }

  if(printhelp == 1){
    printf("Synopsis %s\n", argv[0]);
    for(int m = 0; m < opt_all->module_count; m++ ){
      option_help * args = opt_all->modules[m].options;
      if(args == NULL) continue;
      char * prefix = opt_all->modules[m].prefix;
      if(prefix != NULL){
        printf("\n\nModule %s\n", prefix);
      }
      option_print_help(args);
    }
    exit(EXIT_FAILURE);
  }

  return i;
}
