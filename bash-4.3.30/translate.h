#ifndef __TRANSLATE_H__
#define __TRANSLATE_H__

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "command.h"

extern FILE *outputF;

void print_translation(COMMAND* command);
void initialize_translator();
void close_translator();

#endif //__TRANSLATE_H__
