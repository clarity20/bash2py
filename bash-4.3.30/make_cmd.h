/* make_cmd.h -- Declarations of functions found in make_cmd.c */

/* Copyright (C) 1993-2009 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Bash is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash.  If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined (_MAKE_CMD_H_)
#define _MAKE_CMD_H_

#include "stdc.h"

extern void cmd_init __P((void));

extern WORD_DESC *alloc_word_desc __P((void));
extern WORD_DESC *make_bare_word __P((const char *));
extern WORD_DESC *make_word_flags __P((WORD_DESC *, const char *));
extern WORD_DESC *make_word __P((const char *
#ifdef BASH2PY
                                             , POSITION *positionP
#endif
                               ));
extern WORD_DESC *make_word_from_token __P((int
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                          ));

extern WORD_LIST *make_word_list __P((WORD_DESC *, WORD_LIST *));

#ifdef BASH2PY
#define add_string_to_list(s, l, p) make_word_list (make_word(s,p), (l))
#else
#define add_string_to_list(s, l) make_word_list (make_word(s), (l))
#endif

extern COMMAND *make_command __P((enum command_type, SIMPLE_COM *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                ));
extern COMMAND *command_connect __P((COMMAND *, COMMAND *, int));
extern COMMAND *make_for_command __P((WORD_DESC *, WORD_LIST *, COMMAND *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int
#endif
                                    ));
extern COMMAND *make_group_command __P((COMMAND *));
extern COMMAND *make_case_command __P((WORD_DESC *, PATTERN_LIST *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int
#endif
                                     ));
extern PATTERN_LIST *make_pattern_list __P((WORD_LIST *, COMMAND *));
extern COMMAND *make_if_command __P((COMMAND *, COMMAND *, COMMAND *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                   ));
extern COMMAND *make_while_command __P((COMMAND *, COMMAND *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                      ));
extern COMMAND *make_until_command __P((COMMAND *, COMMAND *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                      ));
extern COMMAND *make_bare_simple_command __P((
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                void
#endif
                                            ));
extern COMMAND *make_simple_command __P((ELEMENT, COMMAND *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                       ));
extern void make_here_document __P((REDIRECT *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int
#endif
                                  ));
extern REDIRECT *make_redirection __P((REDIRECTEE, enum r_instruction, REDIRECTEE, int));
extern COMMAND *make_function_def __P((WORD_DESC *, COMMAND *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int, int
#endif
                                     ));
extern COMMAND *clean_simple_command __P((COMMAND *));

extern COMMAND *make_arith_command __P((WORD_LIST *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                      ));

extern COMMAND *make_select_command __P((WORD_DESC *, WORD_LIST *, COMMAND *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int
#endif
                                       ));

#if defined (COND_COMMAND)
extern COND_COM *make_cond_node __P((int, WORD_DESC *, COND_COM *, COND_COM *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                   ));
extern COMMAND *make_cond_command __P((COND_COM *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                     ));
#endif

extern COMMAND *make_arith_for_command __P((WORD_LIST *, COMMAND *, 
#ifdef BASH2PY
                                                POSITION *positionP
#else
                                                int
#endif
                                          ));

extern COMMAND *make_subshell_command __P((COMMAND *));
extern COMMAND *make_coproc_command __P((char *, COMMAND *
#ifdef BASH2PY
                                                , POSITION *positionP
#endif
                                       ));

extern COMMAND *connect_async_list __P((COMMAND *, COMMAND *, int));

#endif /* !_MAKE_CMD_H */
