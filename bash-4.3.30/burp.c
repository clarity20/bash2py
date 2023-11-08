
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

#include "bash2py_alloc.h"

#include "bashansi.h"

#include "fix_string.h"
#include "burp.h"

#define MAX_CALL_DEPTH 64
#define FULL_INDENT 4
#define SMALL_INDENT 2

int g_log_indent=-FULL_INDENT;
char **g_function_stack;
char **g_current_function;
FILE *g_log_stream;
_BOOL g_log_is_on;

_BOOL g_translate_html = 0;

void
burp_reset(burpT *burpP)
{
	memset(burpP->m_P, '\0', burpP->m_max);

	burpP->m_lth = 0;
	burpP->m_indent = 0;
	burpP->m_disable_indent = FALSE;
	burpP->m_ungetc = 0;
}

static void
increase_burp(burpT *burpP)
{
	int	max;

	if (!burpP->m_P) {
		burpP->m_lth = 0;
		burpP->m_max = 0;
		max          = 1024;
		burpP->m_P   = (char *) xmalloc(max);
		if (!burpP->m_P) {
			fprintf(stderr, "Burp can't xmalloc(%d)\n", max);
			assert(FALSE);
			exit(1);
		}
		burpP->m_max = max - 8;
		return;
	}
	max = burpP->m_max << 1;
	if (max & 0x40000000) {
		// Very serious problems trying to print whatever it might be..
		fprintf(stderr,"Burp can't print\n");
		assert(FALSE);
		exit(1);
	}
	burpP->m_P = realloc(burpP->m_P, max);
	if (!burpP->m_P) {
		fprintf(stderr, "Burp can't realloc(%d)\n", max);
		assert(FALSE);
		exit(1);
	}
	burpP->m_max = max - 8;
}

char *
burp_extend(burpT *burpP, int offset, int need)
{
	int		lth;
	char	*P;
	
	lth = burpP->m_lth;
	assert(0 <= offset && offset <= lth);
	while ((burpP->m_max - lth) < need) {
		increase_burp(burpP);
	}
	P = burpP->m_P + offset;
	memmove(P+need, P, lth - offset);
	lth          += need;
	burpP->m_lth  = lth;
	burpP->m_P[lth] = '\0';
	return P;
}
	
void
burpc1(burpT *burpP, const char c)
{
	char	*P;

	if ((burpP->m_max - burpP->m_lth) < 2) {
		increase_burp(burpP);
	}
	P = burpP->m_P + burpP->m_lth;
	*P++ = c;
	burpP->m_lth++;
	*P   = '\0';
	return;
}

static void
indentation(burpT *burpP)
{
	int i;

	if (burpP->m_lth && burpP->m_P[burpP->m_lth-1] == '\n') {
		if (!burpP->m_disable_indent) {
			assert(0 <= burpP->m_indent);
			for (i = burpP->m_indent * 4; i > 0; --i) {
				burpc1(burpP, ' ');
}	}	}	}

void
burpc(burpT *burpP, const char c)
{
	if (g_translate_html) {
		burpP->m_ungetc = burpP->m_lth;
		switch (c) {
		case '<':
			g_translate_html = FALSE;
			burps(burpP, "&lt;");
			g_translate_html = TRUE;
			return;
		case '>':
			g_translate_html = FALSE;
			burps(burpP, "&gt;");
			g_translate_html = TRUE;
			return;
		case '&':
			g_translate_html = FALSE;
			burps(burpP, "&amp;");
			g_translate_html = TRUE;
			return;
	}	}
	indentation(burpP);
	burpc1(burpP, c);
	assert(burpP->m_lth < burpP->m_max);
}

void
burp_ungetc(burpT *burpP)
{
	assert(burpP->m_ungetc < burpP->m_lth);
	burpP->m_lth = burpP->m_ungetc;
}

void 
burp(burpT *burpP, const char *fmtP, ...)	/* proc */
{
	static burpT	burp_temp = {0,0,0,0,0,0};

	va_list	    arg;
	size_t		size, left;
	int			ret, c;
	char		*P;
	
	va_start(arg, fmtP);
		
	if (!fmtP) {
		fprintf(stderr, "Burp has no format string\n");
		assert(FALSE);
		exit(1);
	}

	indentation(burpP);
	burp_temp.m_lth = 0;
	for (;;) {
		left = burp_temp.m_max - burp_temp.m_lth;
		// Caution: microsoft bug causes ret == -1 if printing any 0xFFFF character
		if (left > 79 && (ret =  vsnprintf(burp_temp.m_P+burp_temp.m_lth, left, fmtP, arg)) < left && 0 <= ret) {
			break;

		}
		increase_burp(&burp_temp);
	}
	burp_temp.m_lth += ret;
	va_end(arg);

	for (P = burp_temp.m_P; c = *P; ++P) {
		burpc(burpP, c);
	}
 	return;
}

void
burpn(burpT *burpP, const char *stringP, int lth)
{
	int	i;

	assert(0 <= lth);
	if (0 < lth) {
		indentation(burpP);
		for (i = 0; i < lth; ++i) {
			burpc(burpP, stringP[i]);
	}	}
	return;
}

void
burps(burpT *burpP, const char *stringP)
{
	burpn(burpP, stringP, strlen(stringP));
}

void
burp_esc_quote(burpT *burpP, int offset, int quote)
{
	char	*P, *P1, *startP;
	int		cnt, need, c;

	cnt = 0;
	for (P = burpP->m_P + offset; c = *P; ++P) {
		if (c == quote) {
			++cnt;
	}	}

	if (cnt) {
	 	need = burpP->m_lth + cnt + 2;
		while (burpP->m_max < need) {
			increase_burp(burpP);
		}
		startP  = burpP->m_P;
		P       = startP + burpP->m_lth;
		P1      = P + cnt;
		startP += offset;
	
		for (; startP <= P; --P) {
			c = *P;
			*P1-- = c;
			if (c == quote) {
				*P1-- = '\\';
		}	}
		assert(P1 = P);
	
		burpP->m_lth += cnt;
	}
}

void
burp_rtrim(burpT *burpP)
{
	int lth;
	
	for (lth = burpP->m_lth; lth && burpP->m_P[lth-1] == ' '; --lth);
	burpP->m_lth = lth;
}

void
burps_html(burpT *burpP, const char *stringP)
{
	int save = g_translate_html;

	g_translate_html = FALSE;
	burps(burpP, stringP);
	g_translate_html = save;
}


void log_init()
{
	g_log_stream = stdout;
	g_log_is_on = FALSE;
	g_function_stack = (char **) malloc(MAX_CALL_DEPTH * sizeof(char **));
	memset(g_function_stack, 0, MAX_CALL_DEPTH * sizeof(g_function_stack[0]));
	g_current_function = g_function_stack-1;
}

void log_close()
{
	while (g_current_function >= g_function_stack)
		free(*g_current_function);
	free(g_function_stack);
}

void log_activate()
{
	g_log_is_on = TRUE;
}

void log_deactivate()
{
	g_log_is_on = FALSE;
}

// convert_format_specifiers(): Implements any printf-like specifiers
// that we choose to invent by rewriting them in terms of standard specifiers
static char * convert_format_specifiers(char *msg)
{
	const int EXTRA_SPACE = 16;
	char specifier[3];
	char *pSpecifier, *converted;
	int len;

	// Convert %q specifiers to "%s" in quote marks
	strcpy(specifier, "%q");
	len = strlen(msg);
	converted = (char *) malloc(len + EXTRA_SPACE);
	strcpy(converted, msg);
	while (pSpecifier = strstr(converted, specifier))
	{
		memmove(pSpecifier+4, pSpecifier+2, (int)((converted+len)-pSpecifier)-1);
		memcpy(pSpecifier, "\"%s\"", 4);
	}

	// All further conversions can be performed here

	return converted;
}

// log_enter(): When invoking this function, invoke log_return() at all possible
// return points in order to keep the logging consistent and avoid memory bugs.
void log_enter(char *format, ...)
{
	va_list args;
	char *pNameEnd;
	char full_format[256];
	int length;

	if (!g_log_stream)
		return;

	format = convert_format_specifiers(format);
	if (!(pNameEnd = strchr(format, '(')))
	{
		fprintf(stderr, "Error in call to log_enter(): Required format is funcname(args)\n");
		free(format);
		return;
	}

	// Internal log bookkeeping
	g_log_indent += FULL_INDENT;	// persistent full indent
	length = (int)(pNameEnd-format);
	g_current_function++;
	*g_current_function = (char *) malloc((length+1)*sizeof(char));
	strncpy(*g_current_function, format, length);
	(*g_current_function)[length]='\0';

	// Printing
	if (g_log_is_on)
	{
		int i;
		sprintf(full_format, "%-*.0dEnter %s", g_log_indent, 0, format);
		if (full_format[strlen(full_format)-1] != '\n')
			strcat(full_format, "\n");
		for (i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
			full_format[i]='|';
		if (i>=FULL_INDENT) full_format[i-FULL_INDENT]='.';

		va_start(args, full_format);
		vfprintf(g_log_stream, full_format, args);
		va_end(args);
	}
	free(format);
}

void log_info(char *format, ...)
{
	va_list args;
	char full_format[256];

	if (!g_log_stream)
		return;

	// No bookkeeping to do -- the internals are transient. Just print.
	if (!g_log_is_on)
		return;

	g_log_indent += SMALL_INDENT;	// transient small indent
	format = convert_format_specifiers(format);
	sprintf(full_format, "%-*.0d%s(): %s", g_log_indent, 0, *g_current_function, format);
	if (full_format[strlen(full_format)-1] != '\n')
		strcat(full_format, "\n");
	for (int i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
		full_format[i]='|';
	g_log_indent -= SMALL_INDENT;

	va_start(args, full_format);
	vfprintf(g_log_stream, full_format, args);
	va_end(args);

	free(format);
}

// log_return: Simple logging and bookkeeping for function returns
void log_return()
{
	log_return_msg(NULL);
}

// A variation of log_return() that allows an arbitrary message upon function return
void log_return_msg(char *msg_template, ...)
{
	char full_entry[256];
	char *msg;
	char entry_format[]="%-*.0dLeave %s() - %s\n";

	if (!g_log_stream)
		return;

	// Construct the log entry, silently ignoring the msg if it is NULL.
	if (g_log_is_on)
	{
		int i;
		if (!msg_template)
		{
			strcpy(entry_format+16, "\n");
			msg = NULL;
		}
		else
		{
			msg_template = convert_format_specifiers(msg_template);
			va_list args;
			va_start(args, msg_template);
			msg = (char *) malloc(256);
			vsprintf(msg, msg_template, args);
			if (msg_template) free(msg_template);
			va_end(args);
		}
		sprintf(full_entry, entry_format, g_log_indent, 0, *g_current_function, msg);
		if (msg) free(msg);
		for (i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
			full_entry[i]='|';
		if (i>=FULL_INDENT) full_entry[i-FULL_INDENT]='`';
		fprintf(g_log_stream, full_entry);
	}

	// Internal log bookkeeping
	free(*g_current_function);
	g_current_function--;
	g_log_indent -= FULL_INDENT;
}

// String conversion utility for better logging.
// THE CALLER NEEDS TO MANAGE THIS MEMORY CAREFULLY!
char *bool_to_text(_BOOL value)
{
	static char text[6];
	strcpy(text, value ? "TRUE" : "FALSE");
	return text;
}

// See bool_to_text() function header
char *type_to_text(fix_typeE value)
{
	static char text[11];
	strcpy(text, value == FIX_INT    ? "_INT" : 
				(value == FIX_STRING ? "_STRING" : 
				(value == FIX_VAR    ? "_VAR" : 
				(value == FIX_NONE   ? "_NONE" : 
									   "_otherType"))));
	return text;
}
#endif // BASH2PY
