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

_BOOL g_translate_html = FALSE;

void burp_reset(burpT *burpP)
{
	memset(burpP->m_P, '\0', burpP->m_max);

	burpP->m_lth = 0;
	burpP->m_indent = 0;
	burpP->m_disable_indent = FALSE;
	burpP->m_ungetc = 0;
}

static void increase_burp(burpT *burpP)
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

char * burp_extend(burpT *burpP, char *text)
{
	return burp_insert(burpP, burpP->m_lth, text);
}

char * burp_insert(burpT *burpP, int offset, char *text)
{
	int		lth = burpP->m_lth;
	int		need = strlen(text);
	char	*P;

	assert(0 <= offset && offset <= lth);
	while ((burpP->m_max - lth) <= need) {
		increase_burp(burpP);
	}
	P = burpP->m_P + offset;
	memmove(P+need, P, lth - offset);
	memcpy(P, text, need);
	lth += need;
	burpP->m_lth = lth;
	burpP->m_P[lth] = '\0';
	return P;
}

void burpc1(burpT *burpP, const char c)
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

static void indentation(burpT *burpP)
{
	int i;

	if (burpP->m_lth && burpP->m_P[burpP->m_lth-1] == '\n') {
		if (!burpP->m_disable_indent) {
			assert(0 <= burpP->m_indent);
			for (i = burpP->m_indent * 4; i > 0; --i) {
				burpc1(burpP, ' ');
}	}	}	}

void burpc(burpT *burpP, const char c)
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

void burp_ungetc(burpT *burpP)
{
	assert(burpP->m_ungetc < burpP->m_lth);
	burpP->m_lth = burpP->m_ungetc;
}

void burp(burpT *burpP, const char *fmtP, ...)
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

void burpn(burpT *burpP, const char *stringP, int lth)
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

void burps(burpT *burpP, const char *stringP)
{
	burpn(burpP, stringP, strlen(stringP));
}

void burp_esc_quote(burpT *burpP, int offset, int quote)
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

void burp_rtrim(burpT *burpP)
{
	int lth;
	
	for (lth = burpP->m_lth; lth && burpP->m_P[lth-1] == ' '; --lth);
	burpP->m_lth = lth;
}

void burps_html(burpT *burpP, const char *stringP)
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
	{
		fprintf(g_log_stream, "WARNING: Logger has no record of a proper return from function %s,\n", *g_current_function);
		free(*g_current_function);
		g_current_function--;
	}
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


// String conversion utilities for better logging
char *bool_to_text(_BOOL value)
{
	static char text[6];
	strcpy(text, value ? "TRUE" : "FALSE");
	return text;
}

char *type_to_text(fix_typeE value)
{
	static char text[12];
	strcpy(text, value == FIX_INT    ? "_INT" :
				(value == FIX_STRING ? "_STRING" :
				(value == FIX_VAR    ? "_VAR" :
				(value == FIX_NONE   ? "_NONE" :
				(value == FIX_EXPRESSION   ? "_EXPRESSION" :
									   "_otherType")))));
	return text;
}


char *_build_log_entry(char *format, va_list *pArgs)
{
	static char result[128];
	char fmt_piece[128];
	void *junk;
	va_list args;
	int length;

	// Convert format string and arguments
	char *start = format, *end = strchr(format, '%');
	char *pResult = result;

	if (!end)
	{
		strcpy(result, format);
		end = memchr(result, '\0', 128);
		if (*(end-1) != '\n') strcpy(end, "\n");
		return format;
	}

	va_copy(args, *pArgs);
	while (TRUE)
	{
		char *pType;

		end++;
		length = end - start + 1;

		memcpy(fmt_piece, start, length);
		pType = fmt_piece +length - 1;
		fmt_piece[length]='\0';

		switch (*pType)
		{
			case 'q':
				strcpy(fmt_piece+length-2, "'%s'");
				sprintf(pResult, fmt_piece, va_arg(args, char *));
				break;
			case 't':
				*pType = 's';
				sprintf(pResult, fmt_piece, type_to_text(va_arg(args, fix_typeE)));
				break;
			case 'b':
				*pType = 's';
				sprintf(pResult, fmt_piece, bool_to_text(va_arg(args, _BOOL)));
				break;
			default:
				vsprintf(pResult, fmt_piece, args);
				// Advance by one argument
				junk = va_arg(args, void *);
				break;
		}

		pResult = memchr(pResult, '\0', 128);
		start = end+1;
		end = strchr(start, '%');
		if (!end) {
			strcpy(pResult, start);
			end = memchr(pResult, '\0', 128);
			if (*(end-1) != '\n') strcpy(end, "\n");
			break;
		}
	}
	va_end(args);
	return result;
}

// log_enter(): When invoking this function, invoke log_return() at all possible
// return points in order to keep the logging consistent and avoid memory bugs.
void log_enter(char *format, ...)
{
	va_list args;
	char *log_text, *pNameEnd;
	int length;

	if (!g_log_stream)
		return;

	va_start(args, format);
	log_text = _build_log_entry(format, &args);
	va_end(args);

	if (!(pNameEnd = strchr(log_text, '(')))
	{
		fprintf(stderr, "Error passing format '%s' to log_enter(): Must have the form 'function_name (args...)'\n", format);
		assert(0);
		return;
	}

	// Internal log bookkeeping: adjust indentation, register current function
	g_log_indent += FULL_INDENT;	// persistent full indent
	length = (int)(pNameEnd-log_text);
	g_current_function++;
	*g_current_function = (char *) malloc((length+1)*sizeof(char));
	strncpy(*g_current_function, log_text, length);
	(*g_current_function)[length]='\0';

	// Printing
	if (g_log_is_on)
	{
		char log_entry[256];
		int i;

		sprintf(log_entry, "%-*.0dEnter %s", g_log_indent, 0, log_text);
		for (i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
			log_entry[i]='|';
		if (i>=FULL_INDENT) log_entry[i-FULL_INDENT]='.';

		fprintf(g_log_stream, log_entry);
	}
}

void log_info(char *format, ...)
{
	va_list args;
	char log_entry[256];
	char *log_text;
	if (!g_log_stream)
		return;

	// No bookkeeping to do -- the internals are transient. Just print.
	if (!g_log_is_on)
		return;

	g_log_indent += SMALL_INDENT;	// transient small indent

	va_start(args, format);
	log_text = _build_log_entry(format, &args);
	va_end(args);

	sprintf(log_entry, "%-*.0d%s(): %s", g_log_indent, 0, *g_current_function, log_text);
	for (int i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
		log_entry[i]='|';
	g_log_indent -= SMALL_INDENT;

	fprintf(g_log_stream, log_entry);
}

// log_return: Simple logging and bookkeeping for function returns
void log_return()
{
	log_return_msg(NULL);
}

// A variation of log_return() that allows an arbitrary message upon function return
void log_return_msg(char *msg_template, ...)
{
	char log_entry[256];
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
			va_list args;
			va_start(args, msg_template);
			msg = _build_log_entry(msg_template, &args);
			va_end(args);
		}

		sprintf(log_entry, entry_format, g_log_indent, 0, *g_current_function, msg);
		for (i=FULL_INDENT-1; i<g_log_indent; i+=FULL_INDENT)
			log_entry[i]='|';
		if (i>=FULL_INDENT) log_entry[i-FULL_INDENT]='`';
		fprintf(g_log_stream, log_entry);
	}

	// Internal log bookkeeping
	free(*g_current_function);
	g_current_function--;
	g_log_indent -= FULL_INDENT;
}

#ifdef TEST
main()
{
	char qstr[] = "test_str";
	int num = 123456;
	fix_typeE fix = FIX_VAR;
	_BOOL fakeBool = TRUE;

	log_init();
	log_activate();
	log_enter("main (qstr=%q, num=%d, bool=%b, fix=%t)", qstr, num, fakeBool, fix);
	log_info("num=%d, bool=%b, fix=%t, qstr=%q", num, fakeBool, fix, qstr);
	log_return_msg("bool=%b, fix=%t, qstr=%q, num=%d", fakeBool, fix, qstr, num);
	log_close();
}
#endif // TEST

#endif // BASH2PY

