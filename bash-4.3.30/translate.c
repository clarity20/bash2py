/* translate.c -- Converting bash commands to corresponding Python statements. */

/* Copyright (C) 1989-2010 Free Software Foundation, Inc.

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

#include "config.h"

#ifdef BASH2PY

#include <stdio.h>
#include <assert.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "flags.h"
#include <y.tab.h>	/* use <...> so we pick it up from the build directory */

#include "shmbutil.h"
#include "burp.h"

#include "builtins/common.h"
#include "fix_string.h"

#include "bash2py_alloc.h"

#if !HAVE_DECL_PRINTF
extern int printf __P((const char *, ...));	/* Yuck.  Double yuck. */
#endif

extern int g_rc_identifier;
extern int g_function_nesting_level;
extern int g_function_parms;

extern translateT	g_translate;

#if defined (PREFER_STDARG)
typedef void PFUNC __P((const char *, ...));

#else
#define PFUNC VFunction
static void fprintf(outputF, );
#endif

#define PRINT_DEFERRED_HEREDOCS(x) \
		do { \
			if (deferred_heredocs) \
			print_deferred_heredocs (x); \
		} while (0)

/* Non-zero means the stuff being printed is inside of a function def. */
static int was_heredoc         = 0;
static int printing_connection = 0;
static int stdout_connection   = 0;
static REDIRECT *deferred_heredocs = 0;
static int g_embedded          = 0;
static int g_started           = 0;

/* A buffer to indicate the indirection level (PS4) when set -x is enabled. */
static char indirection_string[100];

//MIW var declarations begin

extern int   g_translate_html;
static FILE* outputF = 0;

static burpT case_var = {0,0,0,0,0,0};
static burpT temp_burp = {0, 0, 0, 0, 0, 0};
static int first_if = 1;
//MIW var declarations end

burpT	g_output  = {0, 0, 0, 0, 0, 0};

static burpT	g_comment = {0, 0, 0, 0, 0, 0};
static burpT	g_temp    = {0, 0, 0, 0, 0, 0};

typedef enum scopeTypeE {
	sc_local,
	sc_explicitGlobal,   // created inside a nonlocal scope, requiring keyword global
	sc_implicitGlobal
} scopeTypeE;

typedef struct function_nameS {
	struct function_nameS *m_nextP;
	char	*m_nameP;
	int		m_local;
} function_nameT;

function_nameT *g_function_namesP = 0;

static int
is_internal_function(char *nameP)
{
	function_nameT	*function_nameP;

	for (function_nameP = g_function_namesP; function_nameP; function_nameP = function_nameP->m_nextP) {
		if (!strcmp(nameP, function_nameP->m_nameP)) {
			return(1);
	}	}
	return (0);
}

static int capture_globals = 0;
static function_nameT *g_globalsP = 0;

void
seen_global(const char *nameP, int local)
{
	function_nameT **tailPP, *globalP;
	char *P,*P1;
	int	 c;

	if (capture_globals) {
		if (!*nameP) {
			return;
		}
		for (P1 = (char *) nameP; (c = *P1) && c != '[' && c != '='; ++P1);
		*P1 = 0;
		for (tailPP = &g_globalsP; globalP = *tailPP; tailPP = &(globalP->m_nextP)) {
			if (!strcmp(globalP->m_nameP, nameP)) {
				*((char *) P1) = c;
				return;
		}	}
		globalP = (function_nameT *) xmalloc(sizeof(function_nameT) + strlen(nameP) + 1);
		P = (char *) (globalP+1);
		strcpy(P, nameP);
		globalP->m_nameP = P;
		globalP->m_nextP = 0;
		globalP->m_local = local;
		*tailPP          = globalP;
		*P1 = c;
	}
}

//#define UNCHANGED burps(&g_output, "^^")
#define UNCHANGED

extern POSITION position;
static int comment_byte = -1;

static void
print_heredoc_header (redirect)
REDIRECT *redirect;
{
	int kill_leading;
	char *x;

	kill_leading = redirect->instruction == r_deblank_reading_until;

	/* Here doc header */
	if (redirect->rflags & REDIR_VARASSIGN) {
		burpc(&g_output, '{');
		burps(&g_output, redirect->redirector.filename->word);
		burpc(&g_output, '}');
	} else if (redirect->redirector.dest != 0) {
		burp(&g_output, "%d", redirect->redirector.dest);
	}

	/* If the here document delimiter is quoted, single-quote it. */
	burps(&g_output, "print '''");
}

static void
print_heredoc_body (redirect)
REDIRECT *redirect;
{
	int disable_indent;

	/* Here doc body */
	burps(&g_output, redirect->redirectee.filename->word);

	disable_indent = g_output.m_disable_indent;
	g_output.m_disable_indent = 1;
	burps(&g_output, "'''");
	g_output.m_disable_indent = disable_indent;
}

static void
print_redirection (redirect)
REDIRECT *redirect;
{
	int redirector, redir_fd;
	WORD_DESC *redirectee, *redir_word;

	redirectee = redirect->redirectee.filename;
	redir_fd = redirect->redirectee.dest;

	redir_word = redirect->redirector.filename;
	redirector = redirect->redirector.dest;

	switch (redirect->instruction)
	{
	case r_input_direction:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 0) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, "< ");
		burps(&g_output, redirectee->word);
		break;

	case r_output_direction:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 1) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, "> ");
		burps(&g_output, redirectee->word);
		break;

	case r_inputa_direction:	/* Redirection created by the shell. */
		burpc(&g_output, '&');
		break;

	case r_output_force:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 1) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, ">|");
		burps(&g_output, redirectee->word);
		break;

	case r_appending_to:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 1) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, ">> ");
		burps(&g_output, redirectee->word);
		break;

	case r_input_output:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 1) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, "<> ");
		burps(&g_output, redirectee->word);
		break;

	case r_deblank_reading_until:
	case r_reading_until:
		print_heredoc_header (redirect);
		burpc(&g_output, '\n');
		print_heredoc_body (redirect);
		break;

	case r_reading_string:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burpc(&g_output, '}');
		} else if (redirector != 0) {
			burp(&g_output, "%d", redirector);
		}
		burps(&g_output, "<<< ");
		burps(&g_output, redirect->redirectee.filename->word);
		break;

	case r_duplicating_input:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burp(&g_output, "{%s}<&%d", redir_word->word, redir_fd);
		} else {
			burp(&g_output, "%d<&%d", redirector, redir_fd);
		}
		break;

	case r_duplicating_output:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burp(&g_output, "{%s}>&%d", redir_word->word, redir_fd);
		} else {
			burp(&g_output, "%d>&%d", redirector, redir_fd);
		}
		break;

	case r_duplicating_input_word:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burps(&g_output, "}<&");
			burps(&g_output, redirectee->word);
		} else {
			burp(&g_output, "%d<&%s", redirector, redirectee->word);
		}
		break;

	case r_duplicating_output_word:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burps(&g_output, "}>&");
			burps(&g_output, redirectee->word);
		} else {
			burp(&g_output, "%d>&%s", redirector, redirectee->word);
		}
		break;

	case r_move_input:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burp(&g_output, "{%s}<&%d-", redir_word->word, redir_fd);
		} else {
			burp(&g_output, "%d<&%d-", redirector, redir_fd);
		}
		break;

	case r_move_output:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burp(&g_output, "{%s}>&%d-", redir_word->word, redir_fd);
		} else {
			burp(&g_output, "%d>&%d-", redirector, redir_fd);
		}
		break;

	case r_move_input_word:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burps(&g_output, "}<&");
			burps(&g_output, redirectee->word);
			burpc(&g_output, '-');
		} else {
			burp(&g_output, "%d<&%s-", redirector, redirectee->word);
		}
		break;

	case r_move_output_word:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burps(&g_output, "}>&");
			burps(&g_output, redirectee->word);
			burpc(&g_output, '-');
		} else {
			burp(&g_output, "%d>&%s-", redirector, redirectee->word);
		}
		break;

	case r_close_this:
		if (redirect->rflags & REDIR_VARASSIGN) {
			burpc(&g_output, '{');
			burps(&g_output, redir_word->word);
			burps(&g_output, "}>&-");
		} else {
			burp(&g_output, "%d>&-", redirector);
		}
		break;

	case r_err_and_out:
		burps(&g_output, "&>");
		burps(&g_output, redirectee->word);
		break;

	case r_append_err_and_out:
		burps(&g_output, "&>>");
		burps(&g_output, redirectee->word);
		break;
	}
}

static void
print_heredocs (heredocs)
REDIRECT *heredocs;
{
	REDIRECT *hdtail;

	burpc(&g_output, ' ');
	for (hdtail = heredocs; hdtail; hdtail = hdtail->next)
	{
		print_redirection (hdtail);
		burpc(&g_output, '\n');
	}
	was_heredoc = 1;
}

static void
print_redirection_list (REDIRECT *redirects)
{
	REDIRECT *heredocs, *hdtail, *newredir;

	heredocs = (REDIRECT *)NULL;
	hdtail = heredocs;

	was_heredoc = 0;
	while (redirects)
	{
		/* Defer printing the here documents until we've printed the
	 rest of the redirections. */
		if (redirects->instruction == r_reading_until || redirects->instruction == r_deblank_reading_until)
		{
			newredir = copy_redirect (redirects);
			newredir->next = (REDIRECT *)NULL;
			if (heredocs)
			{
				hdtail->next = newredir;
				hdtail = newredir;
			}
			else
				hdtail = heredocs = newredir;
		}
		else if (redirects->instruction == r_duplicating_output_word && redirects->redirector.dest == 1)
		{
			/* Temporarily translate it as the execution code does. */
			redirects->instruction = r_err_and_out;
			print_redirection (redirects);
			redirects->instruction = r_duplicating_output_word;
		}
		else
			print_redirection (redirects);

		redirects = redirects->next;
		if (redirects) {
			burpc(&g_output, ' ');
		}
	}

	/* Now that we've printed all the other redirections (on one line),
     print the here documents. */
	if (heredocs && printing_connection) {
		deferred_heredocs = heredocs;
	} else if (heredocs) {
		print_heredocs (heredocs);
		dispose_redirects (heredocs);
	}
}

static void
handle_redirection_list (REDIRECT **redirectsPP)
{
	REDIRECT **redirectPP, *redirectP, *heredocs, *hdtail, *newredir;
	int redirector, redir_fd;
	WORD_DESC *redirectee, *redir_word;

	for (redirectPP = redirectsPP; redirectP = *redirectPP; ){

		redirectee = redirectP->redirectee.filename;
		redir_fd   = redirectP->redirectee.dest;
		redir_word = redirectP->redirector.filename;
		redirector = redirectP->redirector.dest;

		switch(redirectP->instruction) {
		case r_reading_until:
		case r_deblank_reading_until:
			print_heredoc_header (redirectP);
			burpc(&g_output, '\n');
			print_heredoc_body (redirectP);
			break;
		case r_duplicating_output_word:
			burp(&g_output, "os.dup2(2, %d)\n", redirectP->redirector.dest);
			break;
		case r_input_direction:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 0) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, "< ");
			burps(&g_output, redirectee->word);
			break;
		case r_output_direction:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 1) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, "> ");
			burps(&g_output, redirectee->word);
			break;
		case r_inputa_direction:	/* Redirection created by the shell. */
			burpc(&g_output, '&');
			break;
		case r_output_force:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 1) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, ">|");
			burps(&g_output, redirectee->word);
			break;
	
		case r_appending_to:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 1) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, ">> ");
			burps(&g_output, redirectee->word);
			break;
	
		case r_input_output:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 1) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, "<> ");
			burps(&g_output, redirectee->word);
			break;
	
		case r_reading_string:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burpc(&g_output, '}');
			} else if (redirector != 0) {
				burp(&g_output, "%d", redirector);
			}
			burps(&g_output, "<<< ");
			burps(&g_output, redirectP->redirectee.filename->word);
			break;
	
		case r_duplicating_input:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burp(&g_output, "{%s}<&%d", redir_word->word, redir_fd);
			} else {
				burp(&g_output, "%d<&%d", redirector, redir_fd);
			}
			break;
	
		case r_duplicating_output:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burp(&g_output, "{%s}>&%d", redir_word->word, redir_fd);
			} else {
				burp(&g_output, "%d>&%d", redirector, redir_fd);
			}
			break;
	
		case r_duplicating_input_word:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burps(&g_output, "}<&");
				burps(&g_output, redirectee->word);
			} else {
				burp(&g_output, "%d<&%s", redirector, redirectee->word);
			}
			break;
	
		case r_move_input:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burp(&g_output, "{%s}<&%d-", redir_word->word, redir_fd);
			} else {
				burp(&g_output, "%d<&%d-", redirector, redir_fd);
			}
			break;
	
		case r_move_output:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burp(&g_output, "{%s}>&%d-", redir_word->word, redir_fd);
			} else {
				burp(&g_output, "%d>&%d-", redirector, redir_fd);
			}
			break;
	
		case r_move_input_word:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burps(&g_output, "}<&");
				burps(&g_output, redirectee->word);
				burpc(&g_output, '-');
			} else {
				burp(&g_output, "%d<&%s-", redirector, redirectee->word);
			}
			break;
	
		case r_move_output_word:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burps(&g_output, "}>&");
				burps(&g_output, redirectee->word);
				burpc(&g_output, '-');
			} else {
				burp(&g_output, "%d>&%s-", redirector, redirectee->word);
			}
			break;
	
		case r_close_this:
			if (redirectP->rflags & REDIR_VARASSIGN) {
				burpc(&g_output, '{');
				burps(&g_output, redir_word->word);
				burps(&g_output, "}>&-");
			} else {
				burp(&g_output, "%d>&-", redirector);
			}
			break;
	
		case r_err_and_out:
			burps(&g_output, "&>");
			burps(&g_output, redirectee->word);
			break;
	
		case r_append_err_and_out:
			burps(&g_output, "&>>");
			burps(&g_output, redirectee->word);
			break;
		}
		redirectPP = &redirectP->next;
		continue;
delete_redirection:
		*redirectPP = redirectP->next;
	}
}

// Removes any initial and final quoting from the input string

static char *
translate_dequote(char *P)
{
	char	*P1;

	switch (*P) {
	case '\'':
	case '"':
		P1 = strchr(P+1, *P);
		if (P1 && !P1[1]) {
			*P1 = 0;
			++P;
	}	}
	return P;
}

int 
has_equal_sign(SIMPLE_COM *simple_command)
{
	char* word = simple_command->words->word->word;

	if (strchr(word, '=')) {
		return 1;
	}
	return 0;
}

static void
print_popen_flags(REDIRECT *redirects, int printing)
{
	fix_typeE	got;
	char		*wordP;

	for (; redirects; redirects = redirects->next) {
		switch( redirects->instruction) {
		case r_reading_until:
		case r_deblank_reading_until:
		case r_inputa_direction:
		case r_reading_string:
			burps(&g_output, ",stdin=subprocess.PIPE");
			break;
		case r_input_direction:
			burps(&g_output, ",stdin=file(");
        	wordP = redirects->redirectee.filename->word;
            wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
			burps(&g_output, ",'rb')");
			break;
		case r_duplicating_output:
			burps(&g_output, ",stderr=");
			if (!stdout_connection) {
				burps(&g_output, "subprocess.STDOUT");
			} else {
				burp(&g_output, "_rcw%d", stdout_connection);
			}
			break;
		case r_err_and_out:
		case r_duplicating_output_word:
			burps(&g_output, ",stderr=subprocess.STDOUT");
		case r_output_direction:
		case r_output_force:
			switch (redirects->redirector.dest) {
			case 1:
				if (printing) {
					burps(&g_output, ",file");
				} else {
					burps(&g_output, ",stdout");
				}
				break;
			case 2:
				burps(&g_output, ",stderr");
				break;
			default:
				burp(&g_output, ",std%d", redirects->redirector.dest);
			}
			burps(&g_output, "=file(");
        	wordP = redirects->redirectee.filename->word;
            wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
			burps(&g_output, ",'wb')");
			break;
		case r_append_err_and_out:
			burps(&g_output, ",stderr=subprocess.STDOUT");
		case r_appending_to:
			switch (redirects->redirector.dest) {
			case 1:
				if (printing) {
					burps(&g_output, ",file");
				} else {
					burps(&g_output, ",stdout");
				}
				break;
			case 2:
				burps(&g_output, ",stderr");
				break;
			default:
				burp(&g_output, ",std%d", redirects->redirector.dest);
			}
			burps(&g_output, "=file(");
        	wordP = redirects->redirectee.filename->word;
            wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
			burps(&g_output, ",'ab')");
			break;
		}
	}
}

static int
print_popen_redirection (REDIRECT *redirect)
{
	int redirector, redir_fd;
	WORD_DESC *redirectee, *redir_word;
	char	*wordP;
	fix_typeE	got;
	int	communicates = 0;

	redirectee = redirect->redirectee.filename;
	redir_fd = redirect->redirectee.dest;

	redir_word = redirect->redirector.filename;
	redirector = redirect->redirector.dest;

	switch (redirect->instruction)
	{
	case r_deblank_reading_until:
	case r_reading_until:
    {
		int 	lth, c;
		char	*P;

		communicates = 1;
		burp(&g_output,"_rc%d.communicate(", g_rc_identifier);
		lth = g_output.m_lth;
		burpc(&g_output, '"');
		for (P = redirect->redirectee.filename->word; c = *P; ++P) {
			switch (c) {
			case '\n':
				burps(&g_output, "\\n");
				continue;
			case '"':
			case '\\':
				burpc(&g_output, '\\');
				break;
			}
			burpc(&g_output, c);
		}
		burpc(&g_output, '"');
		if (!(redirect->redirectee.filename->flags &  W_QUOTED)) {
			g_output.m_lth = lth;
			wordP = g_output.m_P + lth;
			wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
		}
		burps(&g_output,")\n");
		break;
    }
	case r_reading_string:
		communicates = 1;
		burp(&g_output,"_rc%d.communicate(", g_rc_identifier);
		wordP = redirect->redirectee.filename->word;
		wordP = fix_string(wordP, FIX_STRING, &got);
		burps(&g_output, wordP);
		burps(&g_output,"+'\\n')\n");
		break;
	}
	if (communicates) {
		burp(&g_output, "_rc%d = _rc%d.wait()", g_rc_identifier, g_rc_identifier);
	}
	return communicates;
}

static int
print_popen_redirection_list (REDIRECT *redirects)
{
	int	ret = 0;
	for (; redirects; redirects = redirects->next) {
		ret |= print_popen_redirection(redirects);
	}
	return ret;
}

/* Print heredocs that are attached to the command before the connector
   represented by CSTRING.  The parsing semantics require us to print the
   here-doc delimiters, then the connector (CSTRING), then the here-doc
   bodies.  We don't print the connector if it's a `;', but we use it to
   note not to print an extra space after the last heredoc body and
   newline. */

static void
print_deferred_heredocs (cstring)
const char *cstring;
{
	REDIRECT *hdtail;

	for (hdtail = deferred_heredocs; hdtail; hdtail = hdtail->next) {
		burpc(&g_output, ' ');
		print_heredoc_header (hdtail);
	}
	if (cstring[0] && (cstring[0] != ';' || cstring[1])) {
		burps(&g_output, cstring);
	}
	if (deferred_heredocs) {
		burpc(&g_output, '\n');
	}
	for (hdtail = deferred_heredocs; hdtail; hdtail = hdtail->next) {
		print_heredoc_body (hdtail);
		burpc(&g_output, '\n');
	}
	if (deferred_heredocs) {
		if (cstring && cstring[0] && (cstring[0] != ';' || cstring[1])) {
			burpc(&g_output, ' ');	/* make sure there's at least one space */
		}
		dispose_redirects (deferred_heredocs);
		was_heredoc = 1;
	}
	deferred_heredocs = (REDIRECT *)NULL;
}

static void
newline (string)
char *string;
{
	burpc(&g_output, '\n');
	if (string && *string) {
		burps(&g_output, string);
	}
}

static void
indent (int amount)
{
	int i;

	for (i = 0; amount > 0; amount--) {
		burpc(&g_output, ' ');
    }
}

typedef struct commentS {
	struct commentS *m_nextP;
	int				m_byte;
	char			*m_textP;
} commentT;

commentT *g_comment_headP = 0;
commentT **g_comment_tailPP = &g_comment_headP;

static void
print_comments(int before_byte)
{
	commentT	*commentP;

	while ((commentP = g_comment_headP) && g_comment_headP->m_byte < before_byte) {
		burps_html(&g_output, g_comment_headP->m_textP);
		if (g_translate_html) {
			burps_html(&g_output, "</pre></td></tr><tr><td></td><td><pre>");
		}
		burpc(&g_output, '\n');
		g_comment_headP = g_comment_headP->m_nextP;
		xfree(commentP);
		if (!g_comment_headP) {
			g_comment_tailPP = &g_comment_headP;
}	}	}

static void
translate_unary_operation(char *operatorP, int complex1, char *term1P)
{
	int			left, mode;
	char		*leftP, *file_typeP;
	fix_typeE	got;

	if (!*operatorP) {
		operatorP = "-n";
	}

	if (complex1) {
		leftP = term1P;
	} else {
		if (!strcmp(operatorP, "-t")) {
			left = FIX_INT;
		} else {
			left = FIX_STRING;
		}
		leftP = fix_string(term1P, left, &got);
	}

	if (*operatorP != '-' || operatorP[2]) {
		goto other;
	}

	file_typeP = 0;
	mode       = -1;
	switch (operatorP[1]) {
	case 'a':
	case 'e':
		/* IJD: Really silly -a has two interpretations
				-a x   .. test if file x exists
				x -a x .. x and x
		 */
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.path.exists(%s)", leftP);
		break;
	case 'b':	// File exists and block special file
		file_typeP = "S_ISBLK";
		goto check_file;
	case 'c': 	// File exists and is character special file
		file_typeP = "S_ISCHR";
		goto check_file;
	case 'd': 	// File exists and is a directory 
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.path.isdir(%s)", leftP);
		break;
	case 'f': 	// File exists and a regular file 
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.path.isfile(%s)", leftP);
		break;
	case 'g':	// True if file exists and its set-group-id bit is set
		mode = 02000;
		goto check_mode;
	case 'h': 	// True if file exists and is a symbolic link
	case 'L':	// True if file exists and is a symbolic link
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.path.islink(%s)", leftP);
		break;
	case 'k':	// True if file exists and sticky bit is set
		mode = 01000;
		goto check_mode;
	case 'n':	// True if length of string is not zero
		burp(&g_output, "%s != ''", leftP);
		break;
	case 'p':	// True if file exists and is a named pipe (FIFO)
		file_typeP = "S_ISFIFO";
		goto check_file;
	case 'r':	// True if file is readable
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.access\x03(%s,R_OK)", leftP);
		break;
	case 's':	// True if file exists and has size > 0
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and os.stat(%s).st_size > 0)", leftP, leftP);
		break;
	case 'u':	// True if file exists and its set-user-id bit is set.
		mode = 01000;
check_mode:
		file_typeP = "S_IMODE";
		goto check_file;
	case 'v':	// True if variable defined:
		burp(&g_output, "dir().count(%s) != 0", leftP);
		break;
	case 'w':	// True if file is writable
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.access(%s,W_OK)", leftP);
		break;
	case 'x':	// True if file is executable
		g_translate.m_uses.m_os = 1;
		burp(&g_output, "os.access(%s,X_OK)", leftP);
		break;
	case 'z':	// True if length of string zero
		burp(&g_output, "%s == ''", leftP);
		break;
	case 'G':	// True if file exists and is owned by effective group id
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and os.stat(%s).st_uid == os.getegid())", leftP, leftP);
		break;
	case 'N':	// True if file exists and has been modified since read
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and os.stat(%s).st_mtime > os.stat(%s).st_atime)", leftP, leftP, leftP);
		break;
	case 'S':	// True if file exists and is a socket
		file_typeP = "S_ISSOCK";
check_file:
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and %s(os.stat(%s).st_mode)", leftP, file_typeP, leftP);
		if (mode >= 0) {
			burp(&g_output, " & 0%o", mode);
		}
		burpc(&g_output, ')');
		break;
	case 'O':	// True if file exists and is owned by effective user id
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path_exists(%s) and os.stat(%s).st_uid == os.geteuid())", leftP, leftP);
		break;
	case 'o':
	case 't': 	// Don't know how to handle
	default:
other:	burp(&g_output, "%s %s", operatorP, leftP);
		break;
	}
	return;
}

static void
translate_binary_operation(char *operatorP, int lcomplex, char *term1P, int rcomplex, char *term2P)
{
	static const char *operatorsPP[] = { 
		"-eq",	//  0
		"-ne",	//  1
		"-lt",	//  2
		"-le",	//  3
		"-gt",	//  4
		"-ge",	//  5
		"-ef",	//  6
		"-nt",	//  7
		"-ot",	//  8

		"=", 	//  9
		"<",	// 10
		">",	// 11
		"!=",	// 12
		"==",	// 13
		"=~",	// 14

		"-a",	// 15
		"-o",	// 16
		0
	};

	const char	*compareP;
	char		*leftP, *rightP;
	fix_typeE	left, right, got;
	int			toffset, loffset, i;

	toffset = g_temp.m_lth;
	for (i = 0; compareP = operatorsPP[i]; ++i) {
		if (!strcmp(operatorP, compareP)) {
			break;
	}	}

	switch (i) {
	case 0:	// -eq
		operatorP = "==";
		goto is_int;
	case 1:	// -ne
		operatorP = "!=";
		goto is_int;
	case 2:	// -lt
		operatorP = "<";
		goto is_int;
	case 3:	// -le
		operatorP = "<=";
		goto is_int;
	case 4:	// -gt
		operatorP = ">";
		goto is_int;
	case 5:	// -ge
		operatorP = ">=";
is_int: left  = FIX_INT;
		right = FIX_INT;
		break;

	case 9:	// =
		operatorP = "==";
	case 6:	// -ef
	case 7:	// -nt
	case 8:	// -ot
	case 10:// <
	case 11:// >
	case 12:// !=
	case 13:// ==
	case 14:// =~
		left  = FIX_STRING;
		right = FIX_STRING;
		break;

	case 15:// -a
		operatorP = "and";
		goto is_var;
	case 16:// -o
		operatorP = "or";
		goto is_var;
	default:// other
		operatorP = fix_string(operatorP, FIX_VAR, &got);
is_var:
		right = FIX_VAR;
		left  = FIX_VAR;
	}

	if (lcomplex) {
		leftP   = term1P;
		loffset = -1;
	} else {
		leftP  = fix_string(term1P, left,  &got);
		// Save it since we might call fix_string again
		burpc(&g_temp, 0);
		loffset = g_temp.m_lth;
		burps(&g_temp, leftP);
	}

	if (rcomplex) {
		rightP  = term2P;
	} else {
		rightP  = fix_string(term2P, right,  &got);
	}
	if (0 <= loffset) {
		// Earlier call might have changed g_temp.m_P
		leftP = g_temp.m_P + loffset;
	}

	switch (i) {
	case 6:	// -ef
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and os.path.exists(%s) and os.stat(%s).st_dev == os.stat(%s).st_dev and os.stat(%s).st_ino == os.stat(%s).st_ino)", leftP, rightP, leftP, rightP, leftP, rightP);
		return;
	case 7: // -nt
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and (not os.path.exists(%s) or os.stat(%s).st_mtime > os.stat(%s).st_mtime))", leftP, rightP, leftP, rightP);
		return;
	case 8:	// -ot
		g_translate.m_uses.m_os   = 1;
		g_translate.m_uses.m_stat = 1;
		burp(&g_output, "(os.path.exists(%s) and (not os.path.exists(%s) or os.stat(%s).st_mtime > os.stat(%s).st_mtime))", rightP, leftP, rightP, leftP);
		return;
	case 14: // [[ x =~ y ]]
		g_translate.m_uses.m_re = 1;
		burp(&g_output, "re.search(r%s,%s)", rightP, leftP);
		break;
	default:
		burp(&g_output, "%s %s %s", leftP, operatorP, rightP);
		return;
	}
	g_temp.m_lth = toffset;
}

static void emit_command (COMMAND *command);

static int
emit_embedded_command(COMMAND *commandP)
{
	int	lth;

	emit_command (commandP);
	for (lth = g_output.m_lth; lth && g_output.m_P[lth-1] == '\n'; --lth);
	g_output.m_lth    = lth;
	g_output.m_P[lth] = 0;

	return TRUE;
}

void
print_printf_cmd (list, separator)
WORD_LIST *list;
char *separator;
{
	WORD_LIST 	*w = list;
	char	  	*wordP = w->word->word;
	fix_typeE	got;

	if (!strcmp(wordP, "-v")) {
		w = w->next;
		if (w) {
			wordP = w->word->word;
			burps(&g_output, wordP);
			burps(&g_output, " = ");
			w = w->next;
		}
		if (w) {
			wordP = w->word->word;
			burps(&g_output, fix_string(wordP, FIX_STRING, &got));
		}
		return;
	}

	g_translate.m_uses.m_print = 1;
	burps(&g_output, "print( ");
	wordP = fix_string(wordP, FIX_STRING, &got);
	burps(&g_output, wordP);
	w = w->next;
    if (w) {
		burps(&g_output, " % (");

		for (; w; w = w->next){
			wordP = w->word->word;
			wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
			if (w->next) {
				burps(&g_output, ", ");
			}
		}
		burpc(&g_output, ')');
	}
	burps(&g_output, " )\n");
}

/* export [-fn] [-p] var[=value] */

void
print_export_cmd (list, separator)
WORD_LIST *list;
char *separator;
{
	WORD_LIST	*nodeP;
	char		*wordP, *P;
	fix_typeE	got;

	for (nodeP = list; nodeP; nodeP = nodeP->next) {
		wordP = nodeP->word->word;
		if (*wordP != '-') {
			g_translate.m_uses.m_os = 1;
			burps(&g_output, "os.environ['");
			P = strchr(wordP, '=');
			if (!P) {
				burps(&g_output, wordP);
				burps(&g_output, "'] = ");
				burps(&g_output, wordP);
			} else {
				burpn(&g_output, wordP, P - wordP);
				burps(&g_output, "'] = ");
				burps(&g_output, fix_string(P+1, FIX_STRING, &got));
			}
			break;
	}	}
	return;
}

// This returns true false value

/* IJD:  This is a rather silly unstructured unparsed expression which
	I imagine was later replaced by the better COND syntax.
 */

static void
print_test_command(WORD_LIST *word_listP, int negated)
{
	WORD_LIST	*atP;
	char		*wordP, *operatorP,*leftP;
	int			offset, seen, c;

	if (negated) {
	   	burps(&g_output, " not (");
	}

	operatorP = 0;
	leftP     = 0;
	offset    = g_output.m_lth;
	seen      = 0;
	for (atP = word_listP; atP; atP = atP->next) {
		wordP = atP->word->word;
		switch (c = wordP[0]) {
		case ']':
			if (!wordP[1] && !atP->next) {
				goto done;
			}
			break;
		case '!':
			if (!wordP[1]) {
				burps(&g_output, "not ");
				break;
      }
		case '(':
			if (!wordP[1]) {
				burpc(&g_output, c);
				seen = 0;
				break;
			}
		case ')':
			if (!wordP[1]) {
				++seen;
				burpc(&g_output, c);
				break;
			}
		default:
			if (!leftP) {
				if (!operatorP && c == '-') {
					operatorP = wordP;
				} else {
					leftP = wordP;
					if (operatorP) {
						if (seen) {
							burpc(&g_output, ' ');
						}
						translate_unary_operation(operatorP, 0, leftP);
						++seen;
						operatorP = 0;
						leftP     = 0;
				}	}
				break;
			} 
			if (!operatorP) {
        switch (c) {
        case '-':
        case '!':
        case '=':
        case '<':
				case '>':
				  operatorP = wordP;
					continue;
        default:
					// Very strange
					operatorP = "";
					continue;
			}	}
			if (seen) {
				burpc(&g_output, ' ');
			}
			translate_binary_operation(operatorP, 0, leftP, 0, wordP);
			++seen;
			operatorP = 0;
			leftP     = 0;
			break;
		}
	}
done:
	if (leftP) {
		translate_unary_operation("-n", 0, leftP);
	}

	if (g_output.m_lth == offset) {
		// nothing emitted
		burps(&g_output, "False");
	}

	if (negated) {
		burpc(&g_output, ')');
	}
	return;
}

static void
print_assignment_command(char *nameP, char *end_variableP, char *end_arrayP, char *end_assignmentP, int local)
{
	char		*translationP;
	int 		c;
	fix_typeE	got;

	c              = *end_variableP;
	*end_variableP = 0;
	seen_global(nameP, local);
	if (g_started > 1) {
		g_translate.m_function.m_make = 1;
		burps(&g_output, "Make(\"");
	}
	burps(&g_output, nameP);
	if (g_started > 1) {
		burps(&g_output, "\")");
	}
	*end_variableP = c;
	if (c == '[') {
		burps(&g_output, ".val[");
		assert(end_arrayP);
		c = *end_arrayP;
		*end_arrayP=0;
		translationP = fix_string(end_variableP+1, FIX_EXPRESSION, &got);
		burps(&g_output, translationP);
		*end_arrayP = c;
		end_variableP = end_arrayP;
	}
	++end_assignmentP;
	c = *end_assignmentP;
	*end_assignmentP = 0;
	if (g_started > 1) {
		g_translate.m_value.m_set_value = 1;
		burps(&g_output,".setValue(");
	} else {
		burps(&g_output, end_variableP);
	}
	g_translate.m_value.m_uses = 1;
	if (g_started < 2) {
		burps(&g_output, "Bash2Py(");
	}
	if (c) {
		*end_assignmentP = c;
		burps(&g_output, fix_string(end_assignmentP, FIX_VAR, &got));
	}
#if 0 
      else {
		burps(&g_output, "\"\"");
	}
#endif
	burpc(&g_output, ')');
	return;
}

static int
isAssignment(char *startP, int local)
{
	char	*P, *end_nameP, *end_arrayP;
	int		c, in_array;

	P = startP;
	if ((c = *P) != '_' && !isalpha(c)) {
		return 0;
	}
	for (++P; (c = *P) == '_' || isalnum(c); ++P);
	end_nameP  = P;
	end_arrayP = 0;
	if (c == '[') {
		end_arrayP = endArray(P);
		if (end_arrayP) {
			P = end_arrayP;
			c = *++P;
	}	}

	switch (c) {
    case '+':
	case '-':
		++P;
	}
	if (*P == '=') {
		print_assignment_command(startP, end_nameP, end_arrayP, P, local);
		return 1;
	}
	return(0);
}


static void
print_declare_command(WORD_LIST	*word_listP)
{
	char		*wordP, *P;
	int 		separator, is_int;
	fix_typeE	got;
	
	separator = -1;
	is_int    = 0;
	for (;word_listP = word_listP->next;) {
		wordP = word_listP->word->word;
		if (wordP[0] == '-') {
			if (!strcmp(wordP, "-p")) {
				if (separator == -1) {
					separator = 0;
					g_translate.m_uses.m_print = 1;
					burps(&g_output, "print(");
				}
			} else if (!strcmp(wordP, "-i")) {
				is_int = 1;
			}
			continue;
		}
		if (separator != -1) {
			if (separator) {
				burpc(&g_output, separator);
			}
			P = strchr(wordP, '=');
			if (P) {
				*P = 0;
			}
			burps(&g_output, wordP);
			separator = ',';
			continue;
		}
		if (!strchr(wordP, '=')) {
			burps(&g_output, wordP);
			if (is_int) {
				burps(&g_output, "=0");
			} else {
				burps(&g_output, "=\"\"");
			}
		} else if (!isAssignment(wordP, 0)) {
			// Strange
			wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
		}
		burpc(&g_output, '\n');
	}
	if (separator != -1) {
		if (!separator) {
			burps(&g_output, "dir()");
		}
		burps(&g_output, ")\n");
	}
	return;
}

static void
print_echo_command(WORD_LIST *word_listP, REDIRECT *redirects)
{
	char		*wordP, *P, *P1;
	int			n_flag, e_flag;
	fix_typeE	got;

	int wordLen;
	int quoted_word_count = 0;
	burpT quoted_word_buffer = {0,0,0,0,0,0};

	n_flag = 0;
	e_flag = 0;
	for (; word_listP = word_listP->next; ) {
		wordP = word_listP->word->word;
		if (!strcmp(wordP, "-n")) {
			n_flag = 1;
			continue;
		}
		if (!strcmp(wordP, "-e")) {
			e_flag = 1;
			continue;
		}
		break;
	}
	
	g_translate.m_uses.m_print = 1;
	burps(&g_output, "print(");
	for (; word_listP; word_listP = word_listP->next) {
		wordP = word_listP->word->word;
		// For some bizarre reason 0x1 appears as 0x1 0x1 (perhaps escaped??)
		for (P = P1 = wordP; *P++ = *P1++; ) {
			if (*P == 1 && P[-1] == 1) {
				++P1;
		}	}
		wordP = fix_string(wordP, FIX_VAR, &got);
		wordLen = strlen(wordP);

		// Combine quoted words into quoted sequences
		if (wordP[0]=='\"' && wordP[wordLen-1] == '\"') {
			quoted_word_count++;
			if (quoted_word_count > 1)
				burpc(&quoted_word_buffer, ' ');
			wordP[wordLen-1]='\0';  // remove close quote
			burps(&quoted_word_buffer, wordP+1);
		}
		else {
			if (quoted_word_count > 0) {
				// Quoted word sequence has ended. Dump it and reset.
				burpc(&g_output, '\"');
				burps(&g_output, quoted_word_buffer.m_P);
				burpc(&g_output, '\"');
				quoted_word_count = 0;
				burp_reset(&quoted_word_buffer);
				burpc(&g_output, ',');
			}
			// If the current word is not quoted just dump it
			burps(&g_output, wordP);
			if (word_listP->next) {
				burpc(&g_output, ',');
			}
		}
	}
	if (quoted_word_count > 0) {
		burpc(&g_output, '\"');
		burps(&g_output, quoted_word_buffer.m_P);
		burpc(&g_output, '\"');
		quoted_word_count = 0;
		burp_reset(&quoted_word_buffer);
	}

	if (n_flag) {
		/* Only works with python 3 */
		burps(&g_output, ",end=\"\"");
	}
	print_popen_flags(redirects, 1);
	burpc(&g_output, ')');
	return;
}

static void
print_let_command(WORD_LIST *word_listP)
{
	WORD_LIST	*atP, *nextP;
	char 		*wordP;
	fix_typeE	got;

	for (atP = word_listP; atP; atP = nextP) {
		nextP = atP->next;
		if (g_started < 2) {
			burp(&g_output, "_rc%d= not ", g_rc_identifier);
		}
		wordP = atP->word->word;
		wordP = fix_string(wordP, FIX_EXPRESSION, &got);
		for (; isspace(*wordP); ++wordP);
		burps(&g_output, wordP);
		burp_rtrim(&g_output);
		if (g_started < 2) {
			burpc(&g_output, '\n');
		} else if (nextP) {
			burpc(&g_output, ' ');
	}	}
	if (g_started < 2) {
		g_translate.m_value.m_uses = 1;
		burp(&g_output, "_rc%d = Bash2Py(_rc%d)", g_rc_identifier, g_rc_identifier);
	}
	return;
}

static void
print_read_command(WORD_LIST *word_listP)
{
	char *wordP;
	char *promptP = 0;
	char *targetP = 0;
	
	for (; word_listP = word_listP->next; ) {
		wordP = word_listP->word->word;
		if (wordP[0] != '-') {
			targetP = wordP;
			break;
		}
		switch (wordP[1]) {
		case 'a':
			targetP = word_listP->next->word->word;
			break;
		default:
			for (; *++wordP; ) {
				switch (*wordP) {
				case 'p':
					word_listP = word_listP->next;
					promptP   = word_listP->word->word;
					break;
				case 'i':
				case 'd':
				case 'n':
				case 'N':
				case 't':
				case 'u':
					word_listP = word_listP->next;
					break;
				default:
					continue;
				}
				break;
			}
			if (word_listP) {
				continue;
			}
			break;
		}
		break;
	}
	if (promptP) {
		g_translate.m_uses.m_print = 1;
		burp(&g_output, "print(%s,end=\"\")\n", promptP);
	}
	if (targetP) {
		burps(&g_output, targetP);
		g_translate.m_value.m_uses = 1;
		burps(&g_output, " = Bash2Py(raw_input())");
		return;
	}
	burps(&g_output, "raw_input()");
	return;
}

static void
print_trap_command(WORD_LIST *word_listP)
{
	char	*wordP;
	char	*handlerP = 0;
	char	*sigP     = 0;
	
	// trap [-lp] [arg] [sigspec]
	for (; word_listP = word_listP->next; ) {
		wordP = word_listP->word->word;
		if (*wordP == '-' && (wordP[1] == 'l' || wordP[1] == 'p')) {
			continue;
		}
		if (!handlerP) {
			handlerP = wordP;
			continue;
		}
		if (!sigP) {
			sigP = wordP;
	}	}
	if (!sigP) {
		sigP = handlerP;
		handlerP = "signal.SIG_DFL";
	} else if (*handlerP == 0 || !strcmp(handlerP, "-")) {
		handlerP = "signal.SIG_IGN";
	}
	// signal.signal(signal.SIGALRM, handler)
	g_translate.m_uses.m_signal = 1;
	burps(&g_output, "signal.signal(");
	if (sigP) {
		burps(&g_output, "signal.SIG");
		burps(&g_output, sigP);
	}
	burpc(&g_output, ',');
	if (handlerP) {
		burps(&g_output, handlerP);
	}
	burps(&g_output, ")\n");
	return;
}

static void
print_umask_command(WORD_LIST *word_listP)
{
	/* umask [-p] [-S] [mode] */
	char		*wordP;
	int			p_flag, s_flag;
	fix_typeE	got;
	
	p_flag = 0;
	s_flag = 0;
	for (; word_listP = word_listP->next; ) {
		wordP = word_listP->word->word;
		if (!strcmp(wordP, "-p")) {
			p_flag = 1;
			continue;
		}
		if (!strcmp(wordP, "-S")) {
			s_flag = 1;
			continue;
		}
		break;
	}
	g_translate.m_uses.m_os = 1;
	if (!word_listP) {
		// Return the value of umask
		burp(&g_output, "_rc%d = os.umask(0)\n", g_rc_identifier);
		burp(&g_output, "os.umask(_rc%d)\n", g_rc_identifier);
		g_translate.m_uses.m_print = 1;
		burp(&g_output,"print(oct(_rc%d))", g_rc_identifier);
		return;
	}
	burps(&g_output,"os.umask(");
	for (; word_listP; word_listP = word_listP->next) {
		wordP = word_listP->word->word;
		wordP = fix_string(wordP, FIX_INT, &got);
		if (isdigit(*wordP)) {
			if (*wordP != '0') {
				burpc(&g_output, '0');
		}	}
		burps(&g_output, wordP);
		if (word_listP->next) {
			burpc(&g_output, ',');
	}	}
	burpc(&g_output,')');
	return;
}

static void
print_unset_command(WORD_LIST *word_listP)
{
	/* unset [-fv] [name] */
	char	*wordP;
	char	*separatorP;
	int		f_flag, v_flag;
	
	f_flag = 0;
	v_flag = 0;
	for (; word_listP = word_listP->next; ) {
		wordP = word_listP->word->word;
		if (!strcmp(wordP, "-f")) {
			f_flag = 1;
			continue;
		}
		if (!strcmp(wordP, "-v")) {
			v_flag = 1;
			continue;
		}
		break;
	}
	separatorP = "";
	for (;word_listP; word_listP = word_listP->next) {
		burp(&g_output,"%sdel %s", separatorP, word_listP->word->word);
		separatorP = "\n";
	}
	return;
}

static void
unbrace(void)
{
	char *P, *startP, *endP;
	int	 lth, c, nesting, in_quotes;

	lth    = g_output.m_lth;
	startP = g_output.m_P;
	if (!lth || *startP != '(' || startP[lth-1] != ')') {
		return;
	}
	nesting   = 0;
	in_quotes = 0;
	for (P = startP + 1; ; ++P) {
		c = *P;
		switch (c) {
		case 0:
			return;
		case '"':
			// Ignore brackets in strings (paranoid)
			endP = endQuotedString(P);
			if (!endP) {
				return;
			}
			P = endP;
			continue;
		case '(':
			++nesting;
			continue;
		case ')':
			if (!nesting) {
				break;
			}
			--nesting;
		default:
			continue;
		}
		break;
	}
	if (P[1]) {
		return;
	}
	startP[0] = ' ';
	g_output.m_lth = lth - 1;
	*P = 0;
	return;
}

// This does arithmetic

static void
print_expression(WORD_LIST *word_listP)
{
	int			start_offset;
	WORD_LIST	*atP, *nextP;
	char 		*wordP;
	fix_typeE	got;

	start_offset = g_output.m_lth;
	for (atP = word_listP; atP; atP = nextP) {
		wordP = word_listP->word->word;
    	burps(&g_output, wordP);
		nextP = atP->next;
		if (nextP) {
			burpc(&g_output, ' ');
		}
	}
	wordP = fix_string(g_output.m_P + start_offset, FIX_EXPRESSION, &got);
	for (; isspace(*wordP); ++wordP);
	g_output.m_lth = start_offset;
	burps(&g_output, wordP);
	burp_rtrim(&g_output);
	unbrace();
}

void
print_simple_command (SIMPLE_COM *simple_command)
{
	WORD_LIST	*word_listP;
	char		*wordP;
	int			c, negated;
	fix_typeE	got;

	word_listP = simple_command->words;
	if (!word_listP) {
		return;
	}
	wordP      = word_listP->word->word;
	if (!wordP || !*wordP) {
		return;
	}

	if (isAssignment(wordP, 0)) {
		return;
	}

	negated = (simple_command->flags & CMD_INVERT_RETURN);

	switch (wordP[0]) {
	case ':':
		if (!strcmp(wordP,":")) {
			burps(&g_output, "pass");
			return;
		}
		break;
	case '[':
		if (!strcmp(wordP, "[")) {
			print_test_command(word_listP->next, negated);
			return;
		}
		break;
	case 'b':
		if (!strcmp(wordP, "break")){
			burps(&g_output, "break");
			return;
		}
		break;
	case 'c':
		if (!strcmp(wordP, "continue")){
			burps(&g_output, "continue");
			return;
		}
	
		if (!strcmp(wordP, "cd")){
			g_translate.m_uses.m_os = 1;
			if (word_listP->next) {
				burps(&g_output, "os.chdir(");
				while (word_listP = word_listP->next) {
					wordP = word_listP->word->word;
					wordP = fix_string(wordP, FIX_STRING, &got);
					burps(&g_output, wordP);
					if (word_listP->next) {
						burpc(&g_output,',');
				}	}
				burpc(&g_output, ')');
				return;
			}
			burps(&g_output, "os.chdir(os.path.expanduser('~'))");			
			return;
		}
		break;
	case 'd':
		if (!strcmp(wordP, "declare")) {
			print_declare_command(word_listP);
			return;
		}
		break;
	case 'e':
		if (!strcmp(wordP, "echo")){
			print_echo_command(word_listP, simple_command->redirects);
			return;
		}
	
		if (!strcmp(wordP, "eval")){
			burps(&g_output, "eval(");
			while (word_listP = word_listP->next) {
				wordP = word_listP->word->word;
				wordP = fix_string(wordP, FIX_STRING, &got);
				burps(&g_output, wordP);
				if (word_listP->next) {
					burpc(&g_output, ',');
			}	}
			burpc(&g_output, ')');
			return;
		}
	
		if (!strcmp(wordP, "exit")){
			if (word_listP->next) {
				burps(&g_output, "exit(");
				for(;;) {
					word_listP = word_listP->next;
					wordP = word_listP->word->word;
					wordP = fix_string(wordP, FIX_INT, &got);
					burps(&g_output, wordP);
					if (!word_listP->next) {
						break;
					}
					burpc(&g_output, ' ');
				}
				burpc(&g_output, ')');
				return;
			}
			burps(&g_output, "exit()");
			return;
		}
	
		if (!strcmp(wordP, "export")){
			print_export_cmd(word_listP->next, " ");
			return;
		}
		break;
	case 'f':
		if (!strcmp(wordP, "false")){
			burps(&g_output, "False");
			return;
		}
		break;
	case 'l':
		if (!strcmp(wordP, "let")){
			print_let_command(word_listP->next);
			return;
		}
	
		if (!strcmp(wordP, "local")) {
			for (; word_listP = word_listP->next;) {
				wordP = word_listP->word->word;
				if (!isAssignment(wordP, 1)) {
					// Strange ....
					wordP = fix_string(wordP, FIX_STRING, &got);
					burps(&g_output, wordP);
				}
			 	if (word_listP->next) {
					burpc(&g_output, '\n');
			}	}
			return;
		}
		if (!strcmp(wordP, "logout")) {
			WORD_LIST *nextP;

			g_translate.m_uses.m_sys = 1;
			burps(&g_output, "sys.exit(");
			for (word_listP = word_listP->next; word_listP; word_listP = nextP) {
				nextP = word_listP->next;
				wordP = word_listP->word->word;
				wordP = fix_string(wordP, FIX_STRING, &got);
				burps(&g_output, wordP);
				if (nextP) {
					burpc(&g_output, ',');
				}
			}
			burpc(&g_output, ')');
			return;
		}
		break;
	case 'p':
		if (!strcmp(wordP, "printf")){
			print_printf_cmd(word_listP->next, " ");
			return;
		} 
	
		if (!strcmp(wordP, "pwd")){
			g_translate.m_uses.m_os = 1;
			g_translate.m_uses.m_print = 1;
			burps(&g_output, "print(os.getcwd())");
			return;
		} 
		break;
	case 'r':
		if (!strcmp(wordP, "read")){
			print_read_command(word_listP);
			return;
		}
	
		if (!strcmp(wordP, "return")){
			burps(&g_output, wordP);
			while (word_listP = word_listP->next) {
				burpc(&g_output, '(');
				wordP = word_listP->word->word;
				wordP = fix_string(wordP, FIX_INT, &got);
				burps(&g_output, wordP);
				if (word_listP->next) {
					burpc(&g_output, ' ');
				}
				burpc(&g_output, ')');
			}
			return;
		}
		break;
	case 't':
		if (!strcmp(wordP, "test")) {
			print_test_command(word_listP->next, negated);
			return;
		}

		if (!strcmp(wordP, "times")) {
			g_translate.m_uses.m_os = 1;
			burps(&g_output, "print (os.times())\n");
			burp(&g_output, "_rc%d = 0", g_rc_identifier);
			return;
		}
	
		if (!strcmp(wordP, "true")){
			burps(&g_output, "True");
			return;
		}
	
		if (!strcmp(wordP, "trap")) {
			print_trap_command(word_listP);
			return;
		}
		break;
	case 'u':
	
		if (!strcmp(wordP, "umask")) {
			print_umask_command(word_listP);
			return;
		}
	
		if (!strcmp(wordP, "unset")) {
			print_unset_command(word_listP);
			return;
		}
	
		break;
	}
	
	if (is_internal_function(wordP)) {
		burps(&g_output, wordP);
		burpc(&g_output, '(');
		while (word_listP = word_listP->next) {
			wordP = word_listP->word->word;
			wordP = fix_string(wordP, FIX_VAR, &got);
			burps(&g_output, wordP);
			if (word_listP->next) {
				burps(&g_output, ", ");
		}	}
		burpc(&g_output, ')');
		return;
	} 

	if (g_started < 2) {
		burp(&g_output, "_rc%d = ", g_rc_identifier);
	}
	if (negated) {
    	burps(&g_output, "not ");
    }
	if (simple_command->redirects) {
		int communicates;
		int offset, i;

		g_translate.m_uses.m_subprocess = 1;
		burps(&g_output, "subprocess.");
		offset = g_output.m_lth;
		burps(&g_output, "call(");
		for (; word_listP; word_listP = word_listP->next) {
			wordP = word_listP->word->word;
			wordP = fix_string(wordP, FIX_STRING, &got);
			burps(&g_output, wordP);
			if (word_listP->next) {
				burps(&g_output, " + \" \" + ");
		}	}
		burps(&g_output, ",shell=True");
		print_popen_flags(simple_command->redirects, 0);
		burps(&g_output, ")\n");
		communicates = print_popen_redirection_list(simple_command->redirects);
		if (communicates) {
			burpc(&g_output, ' ');
			for (i = g_output.m_lth-1; offset < i; --i) {
				g_output.m_P[i] = g_output.m_P[i-1];
			}
			memcpy(g_output.m_P+offset, "Popen", 5);
		}
		return;
	} 
	g_translate.m_uses.m_subprocess = 1;
	burps(&g_output, "subprocess.call([");
	for (; word_listP; word_listP = word_listP->next) {
		wordP = word_listP->word->word;
		wordP = fix_string(wordP, FIX_STRING, &got);
		burps(&g_output, wordP);
		if (!word_listP->next) {
			break;
		}
		burpc(&g_output, ',');
	}
	burps(&g_output, "],shell=True)");
	return;
}

void
print_for_command_head (FOR_COM *for_command)
{
	WORD_LIST	*word_listP, *nextP;
	char		*targetP, *wordP, *P, separator, c;
	int			use_glob;
	fix_typeE	got;

	// Need to ensure target exists and is a bash2py object
	targetP = for_command->name->word;
	g_translate.m_function.m_make      = 1;

	burps(&g_output, "for Make(\"");
	burps(&g_output, targetP);
	burps(&g_output, "\").val in ");

	separator = 0;
	for (word_listP = for_command->map_list; word_listP; word_listP = word_listP->next) {
		wordP = word_listP->word->word;
		wordP = fix_string(wordP, FIX_ARRAY, &got);
		if (separator) {
			burpc(&g_output, separator);
		}
		burps(&g_output, wordP);
		separator = '+';
	}
	if (!separator) {
		burps(&g_output, "[]");
	}
	burpc(&g_output, ':');
}

static void
print_for_command (for_command)
FOR_COM *for_command;
{
	print_for_command_head (for_command);
	newline ("");

	INDENT(g_output);
	emit_command (for_command->action);
	PRINT_DEFERRED_HEREDOCS ("");
	OUTDENT(g_output);
}

#if defined (ARITH_FOR_COMMAND)
static void
print_arith_for_command (arith_for_command)
ARITH_FOR_COM *arith_for_command;
{
	print_expression(arith_for_command->init);
	burpc(&g_output, '\n');
	burps(&g_output, "while ");
	print_expression(arith_for_command->test);
	burps(&g_output, ":\n");
	INDENT(g_output);
	emit_command(arith_for_command->action);
	burpc(&g_output, '\n');
	print_expression(arith_for_command->step);
	OUTDENT(g_output);
}
#endif /* ARITH_FOR_COMMAND */

// (( ... ))

#if defined (DPAREN_ARITHMETIC)
void
print_arith_command (WORD_LIST *word_listP)
{
	WORD_LIST	*atP, *nextP;
	char 		*wordP;
	fix_typeE	got;

	for (atP = word_listP; atP; atP = nextP) {
		nextP = atP->next;
		wordP = word_listP->word->word;
		wordP = fix_string(wordP, FIX_EXPRESSION, &got);
		for (; isspace(*wordP); ++wordP);
		burps(&g_output, wordP);
		burp_rtrim(&g_output);
		if (nextP) {
			burpc(&g_output, ' ');
	}	}
	return;
}
#endif

#ifdef SELECT_COMMAND

void
print_select_command_head (select_command)
SELECT_COM *select_command;
{
	WORD_LIST 	*word_listP;
	char		*wordP;
	fix_typeE	got;

	burps(&g_output, "select ");
	burps(&g_output, select_command->name->word);
	burps(&g_output, " in ");
	for (word_listP = select_command->map_list; word_listP; word_listP = word_listP->next) {
		wordP = word_listP->word->word;
		wordP = fix_string(wordP, FIX_STRING, &got);
		burps(&g_output, wordP);
		if (word_listP->next) {
			burpc(&g_output, ' ');
	}	}
}

static void
print_select_command (select_command)
SELECT_COM *select_command;
{
	print_select_command_head (select_command);

	burpc(&g_output, ';');
	newline ("do\n");
	INDENT(g_output);
	emit_command (select_command->action);
	PRINT_DEFERRED_HEREDOCS ("");
	OUTDENT(g_output);
	newline ("done");
}
#endif /* SELECT_COMMAND */

void
print_case_command_head (case_command)
CASE_COM *case_command;
{
	char 		*P;
	fix_typeE	got;

	case_var.m_lth = 0;
	P = fix_string(case_command->word->word, FIX_STRING, &got);
	burps(&case_var, P);
}

static void
print_case_ors(WORD_LIST* patterns){
	WORD_LIST 	*w;
	char		*P;

	for (w = patterns; w; w = w->next){
		if (!strcmp(w->word->word, "*")){
			continue;
		}
		burps(&g_output, " or ");
		burps(&g_output, case_var.m_P);
		burps(&g_output, " == '");
		P = translate_dequote(w->word->word);
		burps(&g_output, P);
		burpc(&g_output, '\'');
	}
}

static void
print_case_clauses (clauses)
PATTERN_LIST *clauses;
{
	char *P;

	while (clauses)
	{
		newline ("");
		if (!strcmp(clauses->patterns->word->word, "*")){
			burps(&g_output, "else:");
		} else {
			if (first_if) {
				burps(&g_output, "if ( ");
				burps(&g_output, case_var.m_P);
				burps(&g_output, " == ");
				first_if  = 0;
			} else{
				burps(&g_output, "elif ( ");
				burps(&g_output, case_var.m_P);
				burps(&g_output, " == ");
			}
			burpc(&g_output, '\'');

			P = translate_dequote(clauses->patterns->word->word);
			burps(&g_output, P);
			burpc(&g_output, '\'');
			print_case_ors(clauses->patterns->next);
			burps(&g_output, "):");

		}
		burpc(&g_output, '\n');

		INDENT(g_output);
		emit_command (clauses->action);
		OUTDENT(g_output);
		PRINT_DEFERRED_HEREDOCS ("");

		clauses = clauses->next;
	}
}

static void
print_case_command (case_command)
CASE_COM *case_command;
{
	print_case_command_head (case_command);
	if (case_command->clauses)
		print_case_clauses (case_command->clauses);
	first_if = 1;
}

static void
print_while_command (while_command)
WHILE_COM *while_command;
{
	burps(&g_output, "while");
	burpc(&g_output, ' ');
	burpc(&g_output, '(');
	emit_embedded_command (while_command->test);
	burps(&g_output, "):\n");
	PRINT_DEFERRED_HEREDOCS ("");
	INDENT(g_output);
	emit_command (while_command->action);
	PRINT_DEFERRED_HEREDOCS ("");
	OUTDENT(g_output);
}

static void
print_until_command (while_command)
WHILE_COM *while_command;
{
	burps(&g_output, "while True:\n");
	PRINT_DEFERRED_HEREDOCS ("");
	INDENT(g_output);
	emit_command (while_command->action);
	burpc(&g_output, '\n');
	burps(&g_output, "if (");
	emit_embedded_command (while_command->test);
	burps(&g_output, "):\n");
	INDENT(g_output);
	burps(&g_output, "break");
	OUTDENT(g_output);
	PRINT_DEFERRED_HEREDOCS ("");
	OUTDENT(g_output);
}

static int is_test_condition_regmatch(IF_COM *if_command)
{
	COMMAND *test = if_command->test;

	return (test->type == cm_cond && 0 == strcmp(test->value.Cond->op->word, "=~"));
}

static void
print_if_command (if_command)
IF_COM *if_command;
{
	/* Normally we begin by burping "if (" straightaway. But if the command
	 is built around a regmatch, it needs to be rearranged to implement the
	 side effects of the shell variable BASH_REMATCH. This requires
	 some alternative processing. */

	int regmatch_in_progress = FALSE;
    int extra_indents = 0;

	if (is_test_condition_regmatch(if_command)) {
		burps(&g_output, "match_object = ");
		regmatch_in_progress = emit_embedded_command(if_command->test);
		newline("");
	}

	burps(&g_output, "if (");

restart:
	if (regmatch_in_progress) {
		burps(&g_output, "match_object):\n");
		regmatch_in_progress = FALSE;
	}
	else {
		emit_embedded_command (if_command->test);
		burps(&g_output, " ):\n");
	}
	/* regmatch_in_progress should be FALSE now. */
	INDENT(g_output);
	emit_command (if_command->true_case);
	PRINT_DEFERRED_HEREDOCS ("");
	OUTDENT(g_output);

	if (if_command->false_case)
	{
		if (if_command->false_case->flags & CMD_ELIF) {
			if_command = if_command->false_case->value.If;
			if (is_test_condition_regmatch(if_command)) {
				newline("else:\n");
				INDENT(g_output);   extra_indents++;
				burps(&g_output, "match_object = ");
				regmatch_in_progress = emit_embedded_command(if_command->test);
				newline("");
				burps(&g_output, "if (");
			}
			else {
				newline("elif (");
			}
			goto restart;
		}
		newline ("else:\n");
		INDENT(g_output);

		emit_command (if_command->false_case);
		PRINT_DEFERRED_HEREDOCS ("");
		OUTDENT(g_output);
	}
	while (extra_indents-- > 0)
		OUTDENT(g_output);
}

#ifdef COND_COMMAND

/* IJD: This is the structured boolean expression */

static void
print_cond_node (COND_COM *cond, int depth)
{	  
	int 		depth1;
	fix_typeE	got;

	depth1 = depth + 1;
	if (cond->flags & CMD_INVERT_RETURN) {
		burps(&g_output, "not ");
	}

	switch (cond->type) {
	case COND_EXPR:
		if (depth) {
			burps(&g_output, "( ");
		}
		print_cond_node (cond->left, depth1);
		if (depth) {
			burps(&g_output, " )");
		}
		break;
	case COND_AND:
		print_cond_node (cond->left,  depth1);
		burps(&g_output, " and ");
		print_cond_node (cond->right, depth1);
		break;
	case COND_OR:
		print_cond_node (cond->left,  depth1);
		burps(&g_output, " or ");
		print_cond_node (cond->right, depth1);
		break;
	case COND_UNARY:
	{
		COND_COM	*leftP;
		char 		*operatorP, *termP;
		int	 		toffset, ooffset, loffset, lcomplex;
		fix_typeE	got;

		toffset   = g_temp.m_lth;
		operatorP = cond->op->word;
		leftP     = cond->left;
		lcomplex  = (leftP->type != COND_TERM);
		if (!lcomplex) {
			termP   = leftP->op->word;
		} else {
			ooffset  = g_output.m_lth;
			print_cond_node(leftP, depth1);
			termP   = g_output.m_P + ooffset;
			// Save it in a temporary buffer
			// We are going clobber this string
			burpc(&g_temp, 0);
			loffset = g_temp.m_lth;
			burps(&g_temp, termP);
			termP   = g_temp.m_P + loffset;
			// Restore output to entry point
			g_output.m_lth = ooffset;
		}
		translate_unary_operation(operatorP, lcomplex, termP);
		// Return any temp space used
		g_temp.m_lth = toffset;
		break;
	}
	case COND_BINARY:
	{
		COND_COM	*leftP, *rightP;
		char		*operatorP, *term1P, *term2P;
		fix_typeE	type;
		int			toffset, ooffset, loffset, roffset, lcomplex, rcomplex;

		toffset     = g_temp.m_lth;
		operatorP   = cond->op->word;
		leftP       = cond->left;
		rightP		= cond->right;
		lcomplex    = (leftP->type  != COND_TERM);
		rcomplex    = (rightP->type != COND_TERM);

		if (!lcomplex) {
			loffset = -1;
			term1P  = leftP->op->word;
		} else {
			ooffset = g_output.m_lth;
			print_cond_node(leftP, depth1);
			term1P  = g_output.m_P + ooffset;
			// Save it in a temporary buffer
			// We are going clobber this string
			burpc(&g_temp, 0);
			loffset = g_temp.m_lth;
			burps(&g_temp, term1P);
			// Restore output to entry point
			g_output.m_lth = ooffset;
		}

		if (!rcomplex) {
			term2P  = rightP->op->word;
		} else {
			ooffset = g_output.m_lth;
			print_cond_node(rightP, depth1);
			term2P  = g_output.m_P + ooffset;
			// Save it in a temporary buffer
			// We are going clobber this string
			burpc(&g_temp, 0);
			roffset = g_temp.m_lth;
			burps(&g_temp, term2P);
			term2P = g_temp.m_P + roffset;
			// Restore output to entry point
			g_output.m_lth = ooffset;
		}

		if (0 <= loffset) {
			// Delay because g_temp.m_P may be changed
			term1P = g_temp.m_P + loffset;
		}
		translate_binary_operation(operatorP, lcomplex, term1P, rcomplex, term2P);
		// Return any temp space used
		g_temp.m_lth = toffset;
		break;
	}
	case COND_TERM:
	{
		char *wordP;

		wordP = cond->op->word;		
		wordP = fix_string(wordP, FIX_VAR, &got);
		burps(&g_output, wordP);
	}}
}

// [[ .... ]]

void
print_cond_command (cond)
COND_COM *cond;
{
	print_cond_node (cond, 0);
}

#endif

static void
print_pipe_command(CONNECTION *connection)
{
	int old_stdout_connection;

	if (deferred_heredocs) {
		fprintf(stderr, "### Can't handle heredocs with | connector\n");
		deferred_heredocs = 0;
	}
	g_translate.m_uses.m_os  = 1;
	g_translate.m_uses.m_sys = 1;
	burp(&g_output, "_rcr%d, _rcw%d = os.pipe()\n", printing_connection, printing_connection);
	burps(&g_output, "if os.fork():\n");
	INDENT(g_output);
	burp(&g_output, "os.close(_rcw%d)\n", printing_connection);
	burp(&g_output, "os.dup2(_rcr%d, 0)\n", printing_connection);
    emit_command (connection->second);
	burpc(&g_output, '\n');
   	OUTDENT(g_output);
	burps(&g_output, "else:\n");
	INDENT(g_output);
	burp(&g_output, "os.close(_rcr%d)\n", printing_connection);
	burp(&g_output, "os.dup2(_rcw%d, 1)\n", printing_connection);
	old_stdout_connection = stdout_connection;
	stdout_connection     = printing_connection;
    emit_command (connection->first);
	stdout_connection     = old_stdout_connection;
	burpc(&g_output, '\n');
	burps(&g_output, "sys.exit(0)\n");
	OUTDENT(g_output);
}

static void
print_async_command(COMMAND *command)
{
	int thread;

	if (command->type == cm_connection) {
		CONNECTION *connection = command->value.Connection;

		if (connection->connector == '&' && !connection->second) {
			/* Don't need a thread to start threads */
			emit_command(command);
			burpc(&g_output, '\n');
			return;
	}	}

	thread  = ++g_translate.m_uses.m_threading;
	if (deferred_heredocs) {
		fprintf(stderr, "### Can't handle heredocs with & connector\n");
		deferred_heredocs = 0;
	}
	burp(&g_output, "def thread%d():\n", thread);
	INDENT(g_output);
	emit_command(command);
	burpc(&g_output, '\n');
	OUTDENT(g_output);
	burp(&g_output, "threading.Thread(target=thread%d).start()\n", thread);
}

static void
print_connection_command(CONNECTION *connection)
{
	printing_connection++;

	switch (connection->connector) {
	case '|':
		if (g_started < 2) {
			burp(&g_output, "_rc%d = ", g_rc_identifier);
		}
		print_pipe_command(connection);
		break;
	case '&':
	{
		if (g_started < 2) {
			burp(&g_output, "_rc%d = ", g_rc_identifier);
		}
		++g_rc_identifier;
		print_async_command(connection->first);
		if (connection->second) {
			burpc(&g_output, '\n');
			if (!g_embedded) {
				g_started = 0;
			}
			emit_command(connection->second);
			burpc(&g_output, '\n');
		}
		break;
	}
	case ';':
		if (g_started < 2) {
			burp(&g_output, "_rc%d = ", g_rc_identifier);
		}
		emit_command (connection->first);
		if (deferred_heredocs == 0) {
			if (was_heredoc == 0) {
				burpc(&g_output, '\n');
			} else {
				was_heredoc = 0;
			}
		} else {
			print_deferred_heredocs (g_function_nesting_level>0 ? "" : ";");
		}
		if (!g_embedded) {
			g_started = 0;
		}
		emit_command (connection->second);
		PRINT_DEFERRED_HEREDOCS ("");
		break;
	/* IJD: This is all very silly.  AND_AND and OR_OR should be viewed as
		boolean predicates computed as part of a condition.  Instead they
		are very different from -a and -o in being viewed as connectors
	 */
	case AND_AND:
		if (deferred_heredocs) {
			fprintf(stderr, "### Can't handle deferred heredocs with && connector\n");
			deferred_heredocs = 0;
		}
		burps(&g_output, "if ");
	    emit_command (connection->first);
		burpc(&g_output, ':');
		INDENT(g_output);
		burpc(&g_output, '\n');
	    emit_command (connection->second);
		OUTDENT(g_output);
		break;
	case OR_OR:
		if (deferred_heredocs) {
			fprintf(stderr, "### Can't handle deferred heredocs with || connector\n");
			deferred_heredocs = 0;
		}
		burps(&g_output, "if not ");
	    emit_command (connection->first);
		burpc(&g_output, ':');
		INDENT(g_output);
		burpc(&g_output, '\n');
	    emit_command (connection->second);
		OUTDENT(g_output);
		break;
	default:
		fprintf(stderr, _("print_command: bad connector `%d'"),
						connection->connector);
		assert(0);
		exit(1);
	}

	printing_connection--;
}

static void
reset_locals ()
{
	g_function_nesting_level   = 0;
	g_output.m_indent   = 0;
	printing_connection = 0;
    stdout_connection   = 0;
	deferred_heredocs   = 0;
}

static void
print_function_def (func)
FUNCTION_DEF *func;
{
	static burpT	save = {0,0,0,0,0,0};
	
	burpT	temp;
	COMMAND *cmdcopy;
	REDIRECT *func_redirects;
	char	 *nameP = func->name->word;
	function_nameT	 *function_nameP;
	function_nameT	 *globalP, *nextP;
	char	 *P;
	int		parm, save_parms;

	function_nameP = (function_nameT *) xmalloc(sizeof(function_nameT) + strlen(nameP) + 1);
	P = (char *) (function_nameP+1);
	strcpy(P, nameP);
	function_nameP->m_nameP = P;
	function_nameP->m_nextP = g_function_namesP;
	g_function_namesP       = function_nameP;
	
	func_redirects = NULL;
	burps(&g_output, "def ");
	burps(&g_output, nameP);
	burps(&g_output, " (");

	temp     = save;
	save     = g_output;
	g_output = temp;
	g_output.m_lth = 0;
	burps(&g_output, "    ");
	INDENT(g_output);

	capture_globals = 1;
	add_unwind_protect (reset_locals, 0);

	g_function_nesting_level++;

	cmdcopy = copy_command (func->command);
	if (cmdcopy->type == cm_group)
	{
		func_redirects = cmdcopy->redirects;
		cmdcopy->redirects = (REDIRECT *)NULL;
	}
	save_parms       = g_function_parms;
	g_function_parms = 0;
	emit_command (cmdcopy->type == cm_group
			        ? cmdcopy->value.Group->command
					: cmdcopy);

	remove_unwind_protect ();

	for (parm = 1; parm <= g_function_parms; ++parm) {
		if (1 < parm) {
			burpc(&save, ',');
		}
		burp(&save,"_p%d", parm);
	}
	g_function_parms = save_parms;
	burps(&save, ") :\n");
	if (g_globalsP) {
		for (globalP = g_globalsP; globalP; globalP = nextP) {
			if (!globalP->m_local) {
				burps(&save, "    global ");
				burps(&save, globalP->m_nameP);
				burpc(&save, '\n');
			}
			nextP = globalP->m_nextP;
			xfree(globalP);
	    }
		g_globalsP = 0;
		burpc(&save, '\n');
	}

	burpc(&g_output, '\n');
	OUTDENT(g_output);
	burps_html(&save, g_output.m_P);
	temp     = save;
	save     = g_output;
	g_output = temp;

	capture_globals = 0;
	g_function_nesting_level--;

	if (func_redirects)
	{ /* { */
		print_redirection_list (func_redirects);
		cmdcopy->redirects = func_redirects;
	}

	dispose_command (cmdcopy);
}

static void
print_group_command (group_command)
GROUP_COM *group_command;
{
	if (g_function_nesting_level > 0) {
		/* This is a group command { ... } inside of a function
	 definition, and should be printed as a multiline group
	 command, using the current indentation. */
		burpc(&g_output, '\n');
		INDENT(g_output);
	}

	emit_command (group_command->command);

	if (g_function_nesting_level > 0) {
		burpc(&g_output, '\n');
		OUTDENT(g_output);
	} else {
		burpc(&g_output, ' ');
	}
}

/* The internal function.  This is the real workhorse. */

static void
emit_command (COMMAND *command)
{
	int	old_embedded, old_started;

	if (command) {
		assert(0 <= command->position.byte);
		print_comments(command->position.byte);

		handle_redirection_list(&command->redirects);
		if (command->flags & CMD_TIME_PIPELINE) {
			UNCHANGED;
			burps(&g_output, "time ");
			if (command->flags & CMD_TIME_POSIX) {
				burps(&g_output, "-p ");
			}
		}

		if (command->flags & CMD_INVERT_RETURN) {
			switch (command->type) {
			case cm_simple:
				command->value.Simple->flags |= CMD_INVERT_RETURN;
				break;
			case cm_connection:
				command->value.Connection->second->flags |= CMD_INVERT_RETURN;
				break;
			default:
				UNCHANGED;
				burps(&g_output, "not ");
		}	}

		old_embedded = g_embedded;
		old_started  = g_started;
		++g_started;
		++g_embedded;
		switch (command->type) {
		case cm_simple:
			print_simple_command (command->value.Simple);
        	handle_redirection_list(&command->value.Simple->redirects);
			break;

		case cm_for:
			print_for_command (command->value.For);
			break;

#if defined (ARITH_FOR_COMMAND)
		case cm_arith_for:
			print_arith_for_command (command->value.ArithFor);
			break;
#endif

#if defined (DPAREN_ARITHMETIC)
		case cm_arith:
			g_embedded = old_embedded;
			print_arith_command (command->value.Arith->exp);
			break;
#endif

#if defined (SELECT_COMMAND)
		case cm_select:
			print_select_command (command->value.Select);
			break;
#endif

		case cm_case:
			print_case_command (command->value.Case);
			break;

		case cm_while:
			print_while_command (command->value.While);
			break;

		case cm_until:
			print_until_command (command->value.While);
			break;

		case cm_if:
			print_if_command (command->value.If);
			break;

#if defined (COND_COMMAND)
		case cm_cond:
			g_embedded = old_embedded;
			if (!old_started) {
				if (0 == strcmp(command->value.Cond->op->word, "=~")) {
					burp(&g_output, "match_object = ");
				} else {
					burp(&g_output, "_rc%d = ", g_rc_identifier);
				}
			}
			print_cond_command (command->value.Cond);
			break;
#endif

		case cm_connection:
			g_embedded = old_embedded;
			print_connection_command (command->value.Connection);
			break;

		case cm_function_def:
			g_embedded = 0;
			print_function_def (command->value.Function_def);
			break;

		case cm_group:
			print_group_command (command->value.Group);
			break;

		case cm_subshell:
			/* IJD: This is all very silly:
				The bash manual says that ( ... ) causes execution in a subshell
				but in practice it seems the main purpose of this operation is
				to do what it looks like doing.. bracketting an expression
			 */
			burpc(&g_output, '(');
			emit_command(command->value.Subshell->command);
			burpc(&g_output, ')');
			break;

		case cm_coproc:
			UNCHANGED;
			burps(&g_output, "coproc ");
			burps(&g_output, command->value.Coproc->name);
			burps(&g_output, "\n");
			INDENT(g_output);
			emit_command (command->value.Coproc->command);
			OUTDENT(g_output);
			break;

		default:
			UNCHANGED;
			command_error ("print_command", CMDERR_BADTYPE, command->type, 0);
			break;
		}
		g_embedded = old_embedded;
		if (command->redirects) {
			burpc(&g_output, ' ');
			print_redirection_list (command->redirects);
		}
	}
}

/* Make a string which is the printed representation of the command
   tree in COMMAND.  We return this string.  However, the string is
   not consed, so you have to do that yourself if you want it to
   remain around. */

char *
make_command_string (command)
COMMAND *command;
{
	was_heredoc = 0;
	deferred_heredocs = 0;
	g_embedded = 0;
	g_started  = 0;
	emit_command (command);
	return (0);
}

/* Print COMMAND (a command tree) on standard output. */

void
print_command (COMMAND *command)
{
	if (g_translate_html) {
		burps_html(&g_output, "<tr><td></td><td><pre>");
	}
	make_command_string (command);
	if (g_translate_html) {
		burps_html(&g_output, "</pre></td></tr>");
	}
	burpc(&g_output, '\n');
}

void 
seen_comment_char(int c)
{
	char *P;

	if (c < 0) {
		/* Start */
			
		comment_byte = position.byte;
		g_comment.m_lth = 0;
		// burpc(&g_comment, '\n');
		c = '#';
	}
	if (c == '\n') {
		commentT *commentP;

		/* End */
		P = g_comment.m_P;
		if (*P != '#' || P[1] != '!') {
			commentT *commentP;

			commentP = (commentT *) xmalloc(sizeof(commentT));
			commentP->m_nextP = 0;
			commentP->m_byte   = comment_byte;
			commentP->m_textP  = P;
			*g_comment_tailPP  = commentP;
			g_comment_tailPP   = &commentP->m_nextP;
			g_comment.m_lth    = 0;
			g_comment.m_max    = 0;
			g_comment.m_P      = 0;
		}
		return;
	}
	if (c) {
		burpc(&g_comment, c);
	}
}

void
initialize_translator(const char *shell_scriptP)
{
	char *filenameP;

	if (!shell_scriptP) {
	  if (g_translate_html) {
		filenameP = "output.html";
	  } else {
		filenameP = "output.py";
	  }
	  outputF = fopen(filenameP, "w");
	  if (!outputF) {
		fprintf(stderr, "Can't open %s\n", filenameP);
		exit(1);
	  }
    } else {
	  char *P;
	  int  lth        = strlen(shell_scriptP);

	  if (lth > 3 && !strcmp(shell_scriptP+lth-3,".py")) {
		fprintf(stderr,"The bash input %s looks like it is already in python\n", shell_scriptP);
		exit(1);
	  }
	  if (lth > 5 && !strcmp(shell_scriptP+lth-3,".html")) {
		fprintf(stderr,"The bash input %s looks like it is already in html\n", shell_scriptP);
		exit(1);
	  }
	  char *filenameP = P = (char *) alloca(lth+6);
	  memcpy(filenameP, shell_scriptP, lth);
	  P = filenameP + lth;
	  if (g_translate_html) {
	  	memcpy(P, ".html", 5);
	  	P += 5;
	  } else {
	  	memcpy(P, ".py", 3);
	  	P += 3;
	  }
	  *P = 0;
	  
	  outputF = fopen(filenameP, "w");
	  if (!outputF) {
		fprintf(stderr, "Can't open %s\n", filenameP);
		exit(1);
	  }
	}
	if (g_translate_html) {
		fprintf(outputF, "<html>\n<body>\n<table>\n<tr><td></td><td>\n");
	}
	
	fprintf(outputF,"#! /usr/bin/env python\n");

	if (g_translate_html) {
		fprintf(outputF, "</td></tr>\n");
	}
}

void
print_translation(COMMAND * command)
{
	print_command(command);
}

static int
allzero(void *startP, int size)
{
	char *P, *endP;

	P    = (char *) startP;
	for (endP = P + size; P < endP; ++P) {
		if (*P) {
			return 0;
	}	}
	return 1;
}

static void
neededUses(void)
{
	if (g_translate.m_expand.m_star ||
		g_translate.m_expand.m_at   || 
		g_translate.m_expand.m_hash) {
		g_translate.m_uses.m_sys = 1;
	}

	if (g_translate.m_expand.m_dollar) {
		g_translate.m_uses.m_os = 1;
	}

	if (g_translate.m_function.m_glob) {
		g_translate.m_uses.m_glob = 1;
	}
}

static void
emitUses(void)
{
	int	separator = ' ';

	if (g_translate.m_uses.m_print) {
		if (g_translate_html) {
			fprintf(outputF, "<tr><td></td><td>");
		}
		fprintf(outputF, "from __future__ import print_function");
		if (g_translate_html) {
			fprintf(outputF, "</td></tr>");
		}
		fprintf(outputF, "\n");
	}

	if (g_translate.m_uses.m_sys ||
		g_translate.m_uses.m_os  ||
		g_translate.m_uses.m_subprocess  ||
		g_translate.m_uses.m_signal      ||
		g_translate.m_uses.m_threading   ||
		g_translate.m_uses.m_glob        ||
		g_translate.m_uses.m_re) {

		if (g_translate_html) {
			fprintf(outputF, "<tr><td></td><td>");
		}
		fprintf(outputF, "import");
		if (g_translate.m_uses.m_sys) {
			fprintf(outputF, " sys");
			separator = ',';
		}
		if (g_translate.m_uses.m_os) {
			fprintf(outputF, "%cos", separator);
			separator = ',';
		}
		if (g_translate.m_uses.m_subprocess) {
			fprintf(outputF, "%csubprocess", separator);
			separator = ',';
		}
		if (g_translate.m_uses.m_signal) {
			fprintf(outputF, "%csignal", separator);
			separator = ',';
		}
		if (g_translate.m_uses.m_threading) {
			fprintf(outputF, "%cthreading", separator);
			separator = ',';
		}
		if (g_translate.m_uses.m_glob) {
			fprintf(outputF, "%cglob", separator);
			separator = ',';
		}
		if (g_translate.m_uses.m_re) {
			fprintf(outputF, "%cre", separator);
			separator = ',';
		}
	
		if (g_translate_html) {
			fprintf(outputF, "</td></tr>");
		}
		fprintf(outputF, "\n");
	}
	
	if (g_translate.m_uses.m_stat) {
		if (g_translate_html) {
			fprintf(outputF, "<tr><td></td><td>");
		}
		fprintf(outputF, "from stat import *");
		if (g_translate_html) {
			fprintf(outputF, "</td></tr>");
		}
		fprintf(outputF, "\n");
	}
}
	
static void
neededExceptions(void)
{
	if (g_translate.m_expand.m_exclamation ||
		g_translate.m_expand.m_underbar ||
		g_translate.m_expand.m_hyphen ||
		g_translate.m_expand.m_qmark ||
		g_translate.m_expand.m_colon_qmark ||
		g_translate.m_expand.m_prefixStar ||
		g_translate.m_expand.m_prefixAt ||
		g_translate.m_expand.m_indicesStar ||
		g_translate.m_expand.m_indicesAt
		) {
		g_translate.m_exception = 1;
	}
}

static void
emitExceptions(void)
{
	if (!g_translate.m_exception) {
		return;
	}

	if (g_translate_html) {
		fprintf(outputF, "<tr><td></td><td><pre>");
	}

	fprintf(outputF, 
		"class Bash2PyException(Exception):\n"
		"  def __init__(self, value=None):\n"
		"    self.value = value\n"
		"  def __str__(self):\n"
        "    return repr(self.value)\n"
		"\n"
	);

	if (g_translate_html) {
		fprintf(outputF, "</pre></td></tr>");
	}
}
	
static void
neededFunctions(void)
{
	if (g_translate.m_expand.m_eq ||
		g_translate.m_expand.m_colon_eq
		) {
		g_translate.m_function.m_set_value = 1;
	}

	if (g_translate.m_function.m_set_value ||
		g_translate.m_expand.m_minus ||
		g_translate.m_expand.m_eq ||
		g_translate.m_expand.m_colon_minus ||
		g_translate.m_expand.m_colon_eq ||
		g_translate.m_expand.m_colon_qmark ||
		g_translate.m_expand.m_colon_plus
		) {
		g_translate.m_function.m_get_value = 1;
	}

	if (g_translate.m_function.m_make ||
		g_translate.m_function.m_get_value
		) {
		g_translate.m_function.m_get_variable = 1;
	}

	if (g_translate.m_expand.m_minus ||
		g_translate.m_expand.m_eq ||
		g_translate.m_expand.m_qmark ||
		g_translate.m_expand.m_plus
		) {
		g_translate.m_function.m_is_defined = 1;
	}

}

static void
emitFunctions(void)
{
	if (allzero(&g_translate.m_function, sizeof(g_translate.m_function))) {
		return;
	}

	if (g_translate_html) {
		fprintf(outputF, "<tr><td></td><td><pre>");
	}
	if (g_translate.m_function.m_is_defined) {
		fprintf(outputF,
			"def IsDefined(name, local=locals()):\n"
			"  return name in globals() or name in local\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_get_variable) {
		fprintf(outputF,
			"def GetVariable(name, local=locals()):\n"
			"  if name in local:\n"
			"    return local[name]\n"
			"  if name in globals():\n"
			"    return globals()[name]\n"
			"  return None\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_make) {
		fprintf(outputF,
			"def Make(name, local=locals()):\n"
			"  ret = GetVariable(name, local)\n"
			"  if ret is None:\n"
            "    ret = Bash2Py(0)\n"
			"    globals()[name] = ret\n"
			"  return ret\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_get_value) {
		fprintf(outputF,
			"def GetValue(name, local=locals()):\n"
			"  variable = GetVariable(name,local)\n"
			"  if variable is None or variable.val is None:\n"
			"    return ''\n"
			"  return variable.val\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_set_value) {
		fprintf(outputF,
			"def SetValue(name, value, local=locals()):\n"
			"  variable = GetVariable(name,local)\n"
			"  if variable is None:\n"
			"    globals()[name] = Bash2Py(value)\n"
			"  else:\n"
			"    variable.val = value\n"
			"  return value\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_str) {
		fprintf(outputF,
			"def Str(value):\n"
			"  if isinstance(value, list):\n"
			"    return \" \".join(value)\n"
            "  if isinstance(value, basestring):\n"
			"    return value\n"
			"  return str(value)\n"
			"\n"
		);
	}

	if (g_translate.m_function.m_array) {
		fprintf(outputF,
			"def Array(value):\n"
			"  if isinstance(value, list):\n"
			"    return value\n"
			"  if isinstance(value, basestring):\n"
			"    return value.strip().split(' ')\n"
			"  return [ value ]\n"
			"\n"
		);
	}
	
	if (g_translate.m_function.m_glob) {
		fprintf(outputF,
			"def Glob(value):\n"
			"  ret = glob.glob(value)\n"
			"  if (len(ret) < 1):\n"
			"    ret = [ value ]\n"
			"  return ret\n"
			"\n"
		);
	}
	
	if (g_translate_html) {
		fprintf(outputF, "</pre></td></tr>");
	}
}
	
static void
neededExpands(void)
{
	if (g_translate.m_expand.m_star) {
		g_translate.m_expand.m_at = 1;
	}
}

static void
emitExpandClass(void)
{
	if (allzero(&g_translate.m_expand, sizeof(g_translate.m_expand))) {
		return;
	}

	if (g_translate_html) {
		fprintf(outputF, "<tr><td></td><td><pre>");
	}
	fprintf(outputF,
		"class Expand(object):\n"
	);

	/* Returns an array */

	if (g_translate.m_expand.m_at) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def at():\n"
			"    if (len(sys.argv) < 2):\n"
			"      return []\n"
			"    return  sys.argv[1:]\n"
		);
	}

	/* If in_quotes should return concatenation of values separated by ' '
	 * Otherwise should return an array
     */
	if (g_translate.m_expand.m_star) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def star(in_quotes):\n"
			"    if (in_quotes):\n"
			"      if (len(sys.argv) < 2):\n"
			"        return \"\"\n"
			"      return \" \".join(sys.argv[1:])\n"
			"    return Expand.at()\n"
		);
	}

	if (g_translate.m_expand.m_hash) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def hash():\n"
			"    return  len(sys.argv)-1\n"
		);
	}

	if (g_translate.m_expand.m_dollar) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def dollar():\n"
			"    return  os.getpid()\n"
		);
	}

	if (g_translate.m_expand.m_exclamation) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def exclamation():\n"
			"    raise Bash2PyException(\"$! unsupported\")\n"
		);
	}

	if (g_translate.m_expand.m_underbar) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def underbar():\n"
			"    raise Bash2PyException(\"$_ unsupported\")\n"
		);
	}

	if (g_translate.m_expand.m_hyphen) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def hyphen():\n"
			"    raise Bash2PyException(\"$- unsupported\")\n"
		);
	}

	if (g_translate.m_expand.m_minus) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def minus(name, value=''):\n"
			"    if (IsDefined(name)):\n"
			"      return GetValue(name)\n"
			"    return value\n"
		);
	}

	if (g_translate.m_expand.m_eq) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def eq(name, value=''):\n"
			"    if (not IsDefined(name)):\n"
			"      SetValue(name, value)\n"
			"      return value\n"
			"    return GetValue(name)\n"
		);
	}

	if (g_translate.m_expand.m_qmark) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def qmark(name, value=None):\n"
			"    if (not IsDefined(name)):\n"
			"      if (value is None or value == ''):\n"
            "        value = \"Bash variable \" + name + \" is not defined\"\n"
			"      raise Bash2PyException(value)\n"
			"    return GetValue(name)\n"
		);
	}

	if (g_translate.m_expand.m_plus) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def plus(name, value=''):\n"
			"    if (not IsDefined(name)):\n"
			"      return ''\n"
			"    return value\n"
		);
	}

	if (g_translate.m_expand.m_colon_minus) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def colonMinus(name, value=''):\n"
			"    ret = GetValue(name)\n"
			"    if (ret is None or ret == ''):\n"
			"		ret = value\n"
			"    return ret\n"
		);
	}

	if (g_translate.m_expand.m_colon_eq) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def colonEq(name, value=''):\n"
			"    ret = GetValue(name)\n"
			"    if (ret is None or ret == ''):\n"
			"      SetValue(name, value)\n"
			"      ret = value\n"
			"    return ret\n"
		);
	}

	if (g_translate.m_expand.m_colon_qmark) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def colonQmark(name, value=None):\n"
			"    ret = GetValue(name)\n"
			"    if (ret is None or ret == ''):\n"
			"      if (value is None || value == ''):\n"
			"        value = \"Bash variable \" + name + \" is undefined or none\"\n"
			"        raise Bash2PyException(value)\n"
			"    return ret\n"
		);
	}

	if (g_translate.m_expand.m_colon_plus) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def colonPlus(name, value=''):\n"
			"    ret = GetValue(name)\n"
			"    if (ret is None or ret == ''):\n"
			"      return ''\n"
			"    return value\n"
		);
	}

	if (g_translate.m_expand.m_prefixStar) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def prefixStar(name):\n"
			"    raise Bash2PyException(\"${!prefix*}  unsupported\")\n"
			"    return ''\n"
		);
	}

	if (g_translate.m_expand.m_prefixAt) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def prefixAt(name):\n"
			"    raise Bash2PyException(\"${!prefix@}  unsupported\")\n"
			"    return ''\n"
		);
	}
	if (g_translate.m_expand.m_indicesStar) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def indicesStar(name):\n"
			"    raise Bash2PyException(\"${!name[*]}  unsupported\")\n"
			"    return ''\n"
		);
	}

	if (g_translate.m_expand.m_indicesAt) {
		fprintf(outputF,
			"  @staticmethod\n"
			"  def indicesAt(name):\n"
			"    raise Bash2PyException(\"${!name[@]}  unsupported\")\n"
			"    return ''\n"
		);
	}

	fprintf(outputF, "\n");

	if (g_translate_html) {
		fprintf(outputF, "</pre></td></tr>");
	}
}

static void
neededBash2Py(void)
{
	if (g_translate.m_value.m_not_null_else) {
		g_translate.m_value.m_is_null = 1;
	}
    if (g_translate.m_function.m_make) {
		g_translate.m_value.m_uses = 1;
	}
}

static void
emitBash2PyClass(void)
{
	if (allzero(&g_translate.m_value, sizeof(g_translate.m_value))) {
		return;
	}

	if (g_translate_html) {
		fprintf(outputF, "<tr><td></td><td><pre>");
	}
	
	fprintf(outputF,
		"class Bash2Py(object):\n"
		"  __slots__ = [\"val\"]\n"
		"  def __init__(self, value=''):\n"
		"    self.val = value\n"
	);

	if (g_translate.m_value.m_set_value) {
		fprintf(outputF,
			"  def setValue(self, value=None):\n"
			"    self.val = value\n"
			"    return value\n"
		);
	}

	if (g_translate.m_value.m_preincrement) {
		fprintf(outputF,
			"  def preinc(self,inc=1):\n"
			"    self.val += inc\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_postincrement) {
		fprintf(outputF,
			"  def postinc(self,inc=1):\n"
			"    tmp = self.val\n"
			"    self.val += inc\n"
			"    return tmp\n"
		);
	}

	if (g_translate.m_value.m_is_null) {
		fprintf(outputF,
			"  def isNull():\n"
			"    return value is None\n"
		);
	}

	if (g_translate.m_value.m_not_null_else) {
		fprintf(outputF,
			"  def notNullElse(value):\n"
			"    if self.isNull():\n"
			"      return value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_plus) {
		fprintf(outputF,
			"  def plus(value):\n"
			"    self.val += value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_minus) {
		fprintf(outputF,
			"  def minus(value):\n"
			"    self.val -= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_multiply) {
		fprintf(outputF,
			"  def multiply(value):\n"
			"    self.val *= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_idivide) {
		fprintf(outputF,
			"  def idivide(value):\n"
			"    self.val //= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_mod) {
		fprintf(outputF,
			"  def mod(value):\n"
			"    self.val %%= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_lsh) {
		fprintf(outputF,
			"  def lsh(value):\n"
			"    self.val <<= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_rsh) {
		fprintf(outputF,
			"  def rsh(value):\n"
			"    self.val >>= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_band) {
		fprintf(outputF,
			"  def band(value):\n"
			"    self.val &= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_bor) {
		fprintf(outputF,
			"  def bor(value):\n"
			"    self.val |= value\n"
			"    return self.val\n"
		);
	}

	if (g_translate.m_value.m_bxor) {
		fprintf(outputF,
			"  def bxor(value):\n"
			"    self.val ^= value\n"
			"    return self.val\n"
		);
	}

	fprintf(outputF, "\n");

	if (g_translate_html) {
		fprintf(outputF, "</pre></td></tr>");
	}
}

void
close_translator()
{
	print_comments(999999999);

	// What do we need
	neededExpands();
	neededFunctions();
	neededBash2Py();
	neededExceptions();
	neededUses();

	emitUses();
	emitExceptions();
	emitBash2PyClass();
	emitFunctions();
	emitExpandClass();

	if (g_output.m_lth) {
		fputs(g_output.m_P, outputF);
	}
	if (g_translate_html) {
		fprintf(outputF, "</body>\n</html>\n");
	}
	fclose(outputF);
}

#endif
