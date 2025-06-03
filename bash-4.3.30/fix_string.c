#include "config.h"

#ifdef BASH2PY

#include <stdio.h>
#include "bashansi.h"
#include <ctype.h>
#include <assert.h>
#include "burp.h"
#include "fix_string.h"

translateT	g_translate = {0,{0,0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0,0},
                            {0,0,0,0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            {0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0}};

static burpT g_buffer = {0,0,0,0,0,0};
static burpT g_new = {0,0,0,0,0,0};
static burpT g_braced = {0,0,0,0,0,0};

_BOOL g_regmatch_special_case = FALSE;
_BOOL g_is_inside_function = FALSE;
int g_function_parms_count  = 0;

int g_rc_identifier   = 0;

static int g_dollar_expr_nesting_level = 0;

extern _BOOL g_translate_html;

extern	void seen_global(const char *nameP, _BOOL local);

char g_regmatch_var_name[] = "BASH_REMATCH";

static void exchange_burp_buffers(burpT *oldP, burpT *newP)
{
	burpT	temp;

	temp   = *oldP;
	*oldP  = *newP;
	*newP  = temp;
}

// Finds and returns end of a quoted string (or NULL if improperly quoted)
char * endQuotedString(char *stringP)
{
	char	*P;
	int		c;

	switch (*stringP) {
	case START_QUOTE:
	{
		int in_string = 1;
		for (P = stringP + 1; ; ++P) {
			switch (c = *P) {
			case '\0':
				return NULL;
			case START_QUOTE:
				++in_string;
				continue;
			case END_QUOTE:
				if (!--in_string) {
					return P;
				}
				continue;
	}	}	}
	case '"':
		for (P = stringP + 1; ; ++P) {
			switch (c = *P) {
			case '\0':
				return NULL;
			case '"':
				return P;
			case '\\':
				if (P[1]) {
					++P;
		}	}	}
	case '\'':
		for (P = stringP + 1; ; ++P) {
			switch (c = *P) {
			case '\0':
				return NULL;
			case '\'':
				return P;
	}	}	}
	return NULL;
}

char * endArray(char *startP)
{
	char	*P, *endP;
	int		c, in_array;
	
	c = *startP;
	if (c != '[') {
		return NULL;
	}
	in_array = 1;
	for (P = startP + 1; ; ++P) {
		switch (c = *P) {
		case '\0':
			return NULL;
		case ']':
			if (--in_array) {
				continue;
			}
			return P;
		case '\'':
		case '"':
		case START_QUOTE:
			endP = endQuotedString(P);
			if (!endP) {
				return NULL;
			}
			P = endP;
			continue;
		case '[':
			++in_array;
		default:
			continue;
}	}	}

/* Returns pointer to character after end of expansion */

char * endExpand(char *startP)
{
	char *endP;
	int	 expand;

	assert(*startP == START_EXPAND);
	expand = 1;
	for (endP = startP+1; expand; ++endP) {
		switch(*endP) {
		case '\0':
			return endP;
		case START_EXPAND:
			++expand;
			break;
		case END_EXPAND:
			--expand;
			break;
	}	}
	return endP;
}

static int read_hex(char **PP, int max_digits)
{
	char *P    = *PP;
	char *endP = P + max_digits;
	int	 ret, c;

	for (ret = 0; P < endP; ++P) {
		c = *P;
		if (c >= 'A' && c <= 'F') {
			ret <<= 4;
			ret += c + 10 - 'A';
			continue;
		}
		if (c >= 'a' && c <= 'f') {
			ret <<= 4;
			ret += c + 10 - 'a';
			continue;
		}
		if (c >= '0' && c <= '9') {
			ret <<= 4;
			ret += c - '0';
			continue;
		}
		break;
	}
	*PP = P;
	return ret;
}
	
static void emit_hex(int c)
{
	int c1;

	burps(&g_new, "\\x");
	c1 = c >> 4;
	if (c1 < 10) {
		c1 += '0';
	} else {
		c1 += 'a'-10;
	}
	burpc(&g_new, c1);
	c1 = c & 0xF;
	if (c1 < 10) {
		c1 += '0';
	} else {
		c1 += 'a'-10;
	}
	burpc(&g_new, c1);
}

// Try to decide if the string is an integer:

char * isInteger(char *startP)
{
	static char	temp[32];

	char *P, *P1, *endP;
	int  base, value, c;

	P1   = temp;
	endP = temp + sizeof(temp) - 1;

	// String rubbish from input

	for (P = startP; c = *P; ++P) {
		switch (c) {
		case START_QUOTE:
		case END_QUOTE:
		case '"':
			continue;
		case '\\':
			if (P[1]) {
				c = *++P;
			}
			break;
		}
		if (endP <= P1) {
			return FALSE;
		}
		*P1++ = c;
	}
	*P1 = '\0';
		
	base = 10;
	P    = temp;
	switch (*P) {
	case '\0':
		return FALSE;
	case '-':
	case '+':
		if (!P[1]) {
			return FALSE;
		}
		++P;
		break;
	}

	if (*P == '0') {
		switch (P[1]) {
		case 'x':
		case 'X':
			P += 2;
			if (!*P) {
				return FALSE;
			}
			value = read_hex(&P, sizeof(temp));
			if (*P) {
				return FALSE;
			}
			return temp;
		default:
			base = 8;
			break;
	}	}

	for (; ; ++P) {
		switch (c = *P) {
		case '\0':
			return temp;
		case '9':
		case '8':
			if (base < 10) {
				return FALSE;
			}
		case '7':
		case '6':
		case '5':
		case '4':
		case '3':
		case '2':
		case '1':
		case '0':
			continue;
		}
		return FALSE;
	}
}

/* Initialise starting g_buffer with content of stringP */

static void string_to_buffer(const char *stringP)
{
	const char	*P0;
	int			lth;

  	for (P0 = stringP; isspace(*P0); ++P0);
	g_buffer.m_lth = 0;
	/* Make sure we have a string created */
	burpc(&g_buffer, ' ');
	g_buffer.m_lth = 0;
	burps(&g_buffer, P0);
	for (lth = g_buffer.m_lth; lth && isspace(g_buffer.m_P[lth-1]); --lth);
  	g_buffer.m_P[lth] = 0;
	g_buffer.m_lth    = lth;
}

/* Replace single quotes by double quotes escaping contents correctly
 * this simplifies things since no longer need to worry about single quote
 * Also handle ANSI-C Quoting translation (3.1.2.4)
 */

static int g_little_endian = -1;

// Convert any single quotes in g_buffer to escaped double quotes in g_new ???
// Strictly alphanum strings should not be changed. What about $$ strings ???
static void replaceSingleQuotes(void)
{
	int 	in_quotes;
	unsigned char c, c1;
	char	*P;

	in_quotes   = 0;
	g_new.m_lth = 0;
	for (P = g_buffer.m_P; c = *((unsigned char *) P); ++P) {
		switch (c) {
		case '"':
			if (!in_quotes) {
				in_quotes = c;   // found an opening double quote, remember it
			} else if (in_quotes == c) {
				in_quotes = 0;   // found a terminating double quote, toss it
			} else {
				burpc(&g_new, '\\');    // found a double quote inside a non-double quote
			}
			break;
		case '\'':
			if (!in_quotes) {
				in_quotes = c;    // found an opening single quote
				c         = '"';
			} else if (in_quotes == c || in_quotes == '$') {
				in_quotes = 0;
				c         = '"';
			}
			break;
		case '$':
			if (in_quotes == '\'') {
				burpc(&g_new, '\\');
				break;
			}
			if (P[1] == '\'') {
				if (!in_quotes) {
					in_quotes = '$';
					++P;
					c         = '"';
					break;
			}	}
			if (P[1] == '"') {
				/* Translate string according to current locale */	
				if (!in_quotes) {
					/* Ignore the $ */
					continue;
			}	}
			break;

		case '\\':
			if (!in_quotes) {
				if (P[1] == '\'') {
					burps(&g_new, "\"\\'\"");
					P += 1;
					continue;
				}
			} else if (in_quotes == '$') {
				switch (P[1]) {
				case '\'':
				case '"':
					burpc(&g_new, c);
					c = *++P;
					break;
				case 'c':
					P += 2;
					c  = *P;
					if (c >= 'a') {
						c -= 'a';
					} else if (c >= 'A') {
						c -= 'A';
					} else {
						continue;
					}
					c += 1;
					emit_hex(c);
					continue;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = *++P - '0';
					if (P[1] >= '0' && P[1] < '8') {
						c <<= 3;
						c += *++P - '0';
					}
					if (P[1] >= '0' && P[1] < '8') {
						c <<= 3;
						c += *++P - '0';
					}
					emit_hex(c);
					continue;
				case 'x':
					P += 2;
					c = read_hex(&P, 2);
					emit_hex(c);
					--P;
					continue;
				case 'u':
					if (g_little_endian < 0) {
						char *endianP = (char *) &g_little_endian;
						g_little_endian = 1;
						g_little_endian = *endianP;
					}
					P += 2;
					c = read_hex(&P, 4);
					if (g_little_endian) {
						emit_hex(c & 0xFF);
						emit_hex(c >> 8);
					} else {
						emit_hex(c >> 8);
						emit_hex(c & 0xFF);
					}
					--P;
					continue;
				case 'U':
					if (g_little_endian < 0) {
						char *endianP = (char *) &g_little_endian;
						g_little_endian = 1;
						g_little_endian = *endianP;
					}
					P += 2;
					c = read_hex(&P, 8);
					if (g_little_endian) {
						emit_hex(c         & 0xFF);
						emit_hex((c >> 8)  & 0xFF);
						emit_hex((c >> 16) & 0xFF);
						emit_hex((c >> 24));
					} else {
						emit_hex((c >> 24));
						emit_hex((c >> 16) & 0xFF);
						emit_hex((c >> 8)  & 0xFF);
						emit_hex(c         & 0xFF);
					}
					--P;
					continue;
			}	}
		case '`':
			if (in_quotes == '\'') {
				burpc(&g_new, '\\');
			}
			break;
		case '\n':
			if (in_quotes == '\'') {
				burp(&g_new, "\\n");
				continue;
			}
			break;
		default:
			if (in_quotes != '\'') {
				break;
			}
			if (!isprint(c)) {
				switch (c) {
				case '\a':
					burps(&g_new, "\\a");
					break;
				case '\b':
					burps(&g_new, "\\b");
					break;
				case '\f':
					burps(&g_new, "\\f");
					break;
				case '\n':
					burps(&g_new, "\\n");
					break;
				case '\r':
					burps(&g_new, "\\r");
					break;
				case '\t':
					burps(&g_new, "\\t");
					break;
				default:
					emit_hex(c);
				}
				continue;
		}	}
		burpc(&g_new, c);
	}
	exchange_burp_buffers(&g_buffer, &g_new);
}

// emitQuoted(): Quotes the supplied string
static void emitQuotedString(char *startP)
{
	char *P;
	int	 c;

	log_enter("emitQuotedString (startP=%q)", startP);
	burpc(&g_new, '"');
	for (P = startP; ; ++P) {
		c = *P;
		switch (c) {
		case '\0':
			burpc(&g_new, '"');
			return;
		case '\\':
			burpc(&g_new, c);
			if (!P[1]) {
				burpc(&g_new, c);
			} else {
				++P;
				burpc(&g_new, *P);
			}
			continue;
		case '"':
			burpc(&g_new, '\\');
		}
		burpc(&g_new, c);
	}
	log_return();
}

// forward declaration
static char *emitSpecial1(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP);

static char * emitSimpleVariable(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P, *endP;
	int			c, len;
	fix_typeE	got;

	got = FIX_NONE;

	P = startP;
	switch (c = *P++) {
	case '*':
		g_translate.m_expand.m_star = TRUE;
		burp(&g_new,"Expand.star(%d)", in_quotes);
		got = (in_quotes ? FIX_STRING : FIX_ARRAY);
		break;
	case '@':
		g_translate.m_expand.m_at = TRUE;
		burp(&g_new,"Expand.at()");
		got = FIX_ARRAY;
		break;
	case '#':
		// Expands to the number of positional parameters
		g_translate.m_expand.m_hash = TRUE;
		burps(&g_new, "Expand.hash()");
		got = FIX_INT;
		break;
	case '$':
		// Expands to the process id of the shell
		g_translate.m_expand.m_dollar = TRUE;
		burps(&g_new, "Expand.dollar()");
		got = FIX_INT;
		break;
	case '!':
		// Expands to the process ID of the background (asynchronous) command
		g_translate.m_expand.m_exclamation = TRUE;
		burps(&g_new, "Expand.exclamation()");
		got = FIX_INT;
		break;
	case '_':
		g_translate.m_expand.m_underbar = TRUE;
		burps(&g_new, "Expand.underbar()");
		got = FIX_STRING;
		break;
	case '-':
		// Expands to the current option flags
		g_translate.m_expand.m_hyphen = TRUE;
		burps(&g_new, "Expand.hyphen()");
		got = FIX_STRING;	// This is a string with the flag chars in it
		break;
	case '0':
		burps(&g_new, "__file__");
		got = FIX_STRING;
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (g_is_inside_function) {
			int 	parameter;

			parameter = c - '0';
			burp(&g_new, "_p%d", parameter);
			if (g_function_parms_count < parameter) {
				g_function_parms_count = parameter;
			}
		} else {
			g_translate.m_uses.m_sys = TRUE;
			burp(&g_new, "sys.argv[%c]", c);
		}
		got = FIX_VAR;
		break;
	case '?':
		burp(&g_new, "_rc%d", g_rc_identifier);
		got = FIX_INT;
		break;
	default:
		if (c != '_' && !isalpha(c)) {
			return NULL;
		}
		len = strlen(g_regmatch_var_name);
		if (0 == strncmp(startP, g_regmatch_var_name, len)) {
		    g_regmatch_special_case = TRUE;
		    burps(&g_new, "match_object.group");
			P += len-1;
			c = *P;
		}
		else {
			burpc(&g_new, c);
			for (; (c = *P) && (c == '_' || isalnum(c)); ++P) {
				burpc(&g_new, c);
			}
			if (!g_dollar_expr_nesting_level) {
				burps(&g_new, ".val");
			}
		}
		got = FIX_VAR;
#ifndef TEST
		*P = c;
#endif
	}
	*gotP = got;
	return P;
}
	
static char * emitString(char *startP, const char *terminatorsP, int in_quotes)
{
	char 		*P, *endP;
	int			c, offset, is_inside_quotes;
	fix_typeE	got;

	log_enter("emitString (startP=%q, terminatorsP=%q, in_quotes=%d)",
			  startP, terminatorsP, in_quotes);
	is_inside_quotes = FALSE;
	for (P = startP; ; ++P) {
		c = *P;
		offset = g_new.m_lth;
		if (is_inside_quotes) {
			burpc(&g_new, '"');
		}
		if (P != startP) {
			burpc(&g_new, '+');	
		}
		endP = emitSpecial1(P, in_quotes, FIX_STRING, &got);
		if (endP) {
			is_inside_quotes = FALSE;
			P = endP - 1;
			continue;
		}
		// Undo emission
		g_new.m_lth = offset;
		if (!c) {
			return NULL;
		}
		if (strchr(terminatorsP, c)) {
			goto done;
		}
		if (!is_inside_quotes) {
			if (P != startP) {
				burpc(&g_new, '+');
			}
			burpc(&g_new, '"');
			is_inside_quotes = TRUE;
		}
		switch(c) {
		case '"':
			burpc(&g_new, '\\');
			break;
		case '\\':
			if (P[1]) {
				c = *++P;
				if (c == '\\') {
					burpc(&g_new, c);
				}
		}	}
		burpc(&g_new, c);
	}
done:
	if (is_inside_quotes) {
		burpc(&g_new, '"');
	}
	log_return();
	return P;
}

static char * emitFunction(char *nameP, char *parm1P, char *parm2P, _BOOL indirect, int in_quotes)
{
	char 		*endP;
	int	 		offset;

	log_enter("emitFunction (nameP=%q, parm1P=%q, parm2P=%q, indirect=%b, in_quotes=%d)",
			nameP, parm1P, parm2P, indirect, in_quotes);

	burp(&g_new, "%s(", nameP);
	if (indirect) {
		g_translate.m_function.m_get_value = TRUE;
		burp(&g_new, "GetValue(%s.val)", parm1P);
	} else {
		emitQuotedString(parm1P);
	}
	if (parm2P) {
		offset = g_new.m_lth;
		burpc(&g_new, ',');
		endP = emitString(parm2P, "[}", in_quotes);
		if (!endP) {
			return NULL;
		}
		if (endP == parm2P) {
			g_new.m_lth = offset;
	}	}
	burpc(&g_new, ')');

	log_return();
	return endP;
}

// Process variable names following $ or ${. Handles variables having the indirection 
// prefix "!" and/or a postfix in the set { [*] [@] :- := :+ :? } while handing off 
// simpler variable name instances to emitSimpleVariable, then finally some postprocessing.

static char * emitVariable(char *startP, _BOOL is_braced, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P, *endP, *functionP, *end_start1P, *start2P;
	int			c, is_array;
	_BOOL		is_indirect;
	fix_typeE	got;

	log_enter("emitVariable (startP=%q, is_braced=%b, in_quotes=%d, want=%t)",
			startP, is_braced, in_quotes, want);
	start2P     = 0;
	functionP   = 0;

	*gotP = FIX_STRING;

	if (is_braced) {

		is_indirect = (*startP == '!');
		if (is_indirect) {
			++startP;
		}
		for (start2P = startP; ; ++start2P) {
			end_start1P = start2P;
			switch(c = *start2P) {
			case '\0':
				log_return_msg("Early return, null character");
				return NULL;
			case '*':
				if (is_indirect == TRUE && start2P[1] == '}') { 	// ${!prefix*}
					g_translate.m_expand.m_prefixStar = TRUE;
					functionP = "Expand.prefixStar";
					is_indirect = FALSE;
				}
				break;
			case '@': 					// ${!prefix@}
				if (is_indirect == TRUE && start2P[1] == '}') {
					g_translate.m_expand.m_prefixAt = TRUE;
					functionP = "Expand.prefixAt";
					is_indirect = FALSE;
				}
				break;
				

			case '[':	// Subscript
				if (is_indirect == TRUE) {
					if (!strncmp(start2P, "[*]}", 4)) {	// ${!name[*]}
						g_translate.m_expand.m_indicesStar = TRUE;
						functionP = "Expand.indicesStar";
						is_indirect = FALSE;
						start2P  += 2;
					}
					if (!strncmp(start2P, "[@]}", 4)) {	// ${!name[@]}
						g_translate.m_expand.m_indicesAt = TRUE;
						functionP = "Expand.indicesAt";
						is_indirect = FALSE;
						start2P  += 2;
					}
				} 
			case '}':	// End of expansion
				break;
			case '-':
				g_translate.m_expand.m_minus = TRUE;
				functionP = "Expand.minus";
				break;
			case '=':
				g_translate.m_expand.m_eq = TRUE;
				functionP = "Expand.eq";
				break;
			case '?':
				g_translate.m_expand.m_qmark = TRUE;
				functionP = "Expand.qmark";
				break;
			case '+':
				g_translate.m_expand.m_plus = TRUE;
				functionP = "Expand.plus";
				break;
			case ':':
				switch (*++start2P) {
				case '-':
					g_translate.m_expand.m_colon_minus = TRUE;
					functionP = "Expand.colonMinus";
					break;
				case '=':
					g_translate.m_expand.m_colon_eq = TRUE;
					functionP   = "Expand.colonEq";
					break;
				case '?':
					g_translate.m_expand.m_colon_qmark = TRUE;
					functionP   = "Expand.colonQmark";
					break;
				case '+':
					g_translate.m_expand.m_colon_plus = TRUE;
					functionP = "Expand.colonPlus";
					break;
				default:
					--start2P;
				}
				break;
			default:
				continue;
			}
			if (!functionP) {
				if (is_indirect) {
					c = *end_start1P;
					*end_start1P = '\0';
					burp(&g_new,"GetValue(%s.val)", startP);
				    *end_start1P = c;
					endP = end_start1P;
					goto done;
				}
			} else {
				assert(start2P);
				++start2P;
				c = *end_start1P;
				*end_start1P = '\0';
				endP = emitFunction(functionP, startP, start2P, is_indirect, in_quotes);
				*end_start1P = c;
				if (!endP) {
					log_return_msg("Early return, null character");
					return NULL;
				}
				goto done;
			}
			break;
	}	}
	endP = emitSimpleVariable(startP, in_quotes, want, gotP);
done:
	if (is_braced) {
		int offset;

		is_array = (*endP == '[' || g_regmatch_special_case);
		if (is_array) {
			*gotP = FIX_VAR;
			if (g_regmatch_special_case) {
				// Python gets the regmatch member by a function call () not a direct lookup []
				*endP = '(';
			}
		}
		for (; ; ++endP) {
			c = *endP;
			offset = g_new.m_lth;
			P = emitSpecial1(endP, in_quotes, want, &got);
			if (P) {
				endP = P - 1;
				continue;
			}
			// Undo emission
			g_new.m_lth = offset;
			switch (c) {
			case '\0':
				log_return_msg("Early return, null character");
				return NULL;
			case '}':
				++endP;
				goto finish;
			case ']':
				if (is_array) {
					// We don't know what an array is of..
					burps(&g_new, g_regmatch_special_case ? ")" : "]");
					is_array = FALSE;
					g_regmatch_special_case = FALSE;
					continue;
				}
			}
			burpc(&g_new, c);
	}	}
finish:
	log_return();
	return endP;
}

/* Process the $((...)) construct */

static char *
emitDollarExpression(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char 		*P, *P1, *endP;
	int  		start, offset, is_blank, c, paren_depth;
	fix_typeE	got;

	log_enter("emitDollarExpression (startP=%q, in_quotes=%d, want=%t)", startP, in_quotes, want);

	++g_dollar_expr_nesting_level;
	*gotP    = FIX_INT;
	paren_depth = 0;
	is_blank    = TRUE;
	start    = g_new.m_lth;
	burpc(&g_new, '(');
	for (P = startP; (c = *P); ++P) {
		if (!isspace(c)) {
			is_blank = FALSE;
			offset = g_new.m_lth;
			endP = emitSpecial1(P, in_quotes, FIX_INT, &got);
			if (endP) {
				P = endP - 1;
				continue;
			}
			// Undo emission
			g_new.m_lth = offset;
		}
		switch (c) {
		case '\0':
			// Allow it not to be terminated with ))
			// Makes some logic simpler
			goto done;
		case '\\':
			if (P[1]) {
				c = *++P;
			}
			break;
		case '"':
			endP = endQuotedString(P);
			if (endP) {
				for (; P < endP; ++P) { 
					burpc(&g_new, *P);
				}
				c = *P;
			}
			break;
		case '(':
			++paren_depth;
			break;
		case ')':
			if (paren_depth > 0) {
				--paren_depth;
				break;
			}
			if (P[1] == ')') {
				P += 2;
				goto done;
			}
			break;
		}
		burpc(&g_new, c);
	}
done:
	if (is_blank) {
		g_new.m_lth = start;
		burpc(&g_new, '0');
	} else {
		burpc(&g_new, ')');
#ifndef TEST
		// Only do for the outermost $((...))
		// Otherwise we will be translating Python output instead of the Bash
		if (g_dollar_expr_nesting_level == 1) {
			// Do allow array
			if (!translate_expression(g_new.m_P + start, &P1, TRUE)) {
				log_return_msg("Early exit after translation");
				return NULL;
			}
			g_new.m_lth = start;
			burps(&g_new, P1);
		}
#endif
	}
	--g_dollar_expr_nesting_level;
	log_return();
	return P;
} 

/* When the old-style backquote form of substitution is used, backslash retains its literal meaning except when followed by ‘$’, ‘`’, or ‘\’. The first backquote not preceded by a backslash terminates the command substitution. When using the $(command) form, all characters between the parentheses make up the command; none are treated specially. 
*/

static char * emitCommand(char *startP, int new_style, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	int			old_dollar_expr_nesting_level;
	char 		*endP;

	old_dollar_expr_nesting_level = g_dollar_expr_nesting_level;
	g_dollar_expr_nesting_level   = 0;
	*gotP = FIX_STRING;
	burps(&g_new, "os.popen(");

	if (new_style) {
		endP = emitString(startP+2, ")", in_quotes);
	} else {
		endP = emitString(startP+1, "`", in_quotes);
	}
	if (endP) {
		++endP;
		g_translate.m_uses.m_os = TRUE;
		burps(&g_new, ").read().rstrip(\"\\n\")");
	}
	g_dollar_expr_nesting_level = old_dollar_expr_nesting_level;
	return endP;
}

static char * emitTilde(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char 		*P, *endP;
	int			c, offset, in_string;
	fix_typeE	got;

	*gotP = FIX_STRING;
	burps(&g_new, "os.path.expanduser(\"~");
	in_string = TRUE;
	for (P = startP+1; ; ++P) {
		offset = g_new.m_lth;
		if (in_string) {
			burpc(&g_new, '"');
			in_string = FALSE;
		}
		burpc(&g_new, '+');
		endP = emitSpecial1(P, in_quotes, FIX_STRING, &got);
		if (endP) {
			P = endP - 1;
			continue;
		}
		// Undo emission
		g_new.m_lth = offset;
		switch (c = *P) {
		case '_':
		case '-':
		case '.':
		case '/':
			break;
		default:
			if (!isalnum(c)) {
				goto done;
		}	}
		if (!in_string) {
			burps(&g_new, "+\"");
			in_string = TRUE;
		}
		burpc(&g_new, c);
	}
done:
	if (in_string) {
		burpc(&g_new, '"');
	}
	burpc(&g_new, ')');
	if (P) {
		g_translate.m_uses.m_os = 1;
	}
	return P;
}

// Identifies tilde, backtick, subshell and double-paren expressions as well as 
// variable names following $ or ${, dispatches each to their appropriate handler,
// and attempts a type-fixing of the result

static char * emitSpecial1(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char *endP, *P;
	int	 c, c1, start;
	
	log_enter("emitSpecial1 (startP=%q, in_quotes=%d, want=%t)", startP, in_quotes, want);

	endP   = NULL;
	start  = g_new.m_lth;

	switch (c = *startP) {
	case '~':
		endP = emitTilde(startP, in_quotes, want, gotP);
		if (gotP) {
			*gotP = FIX_STRING;
		}
		break;
	case '`':
		endP = emitCommand(startP, FALSE, in_quotes, want, gotP);
		if (gotP) {
			*gotP = FIX_STRING;
		}
		break;
	case '$':
		switch (startP[1]) {
		case '(':
			if (startP[2] == '(') {
				endP = emitDollarExpression(startP+3, in_quotes, want, gotP);
				if (gotP) {
					*gotP = FIX_INT;
				}
			} else {
				endP = emitCommand(startP, TRUE, in_quotes, want, gotP);
				if (gotP) {
					*gotP = FIX_STRING;
			}	}
			break;
		case '{':
			endP = emitVariable(startP+2, TRUE, in_quotes, want, gotP);
			break;
		default:
			endP = emitVariable(startP+1, FALSE, in_quotes, want, gotP);
			break;
		}
		break;
	}
	if (gotP) {
		fix_typeE got;

		got = *gotP;
		switch (want) {
		case FIX_STRING:
			switch (got) {
			case FIX_STRING:
				break;
			case FIX_INT:
			case FIX_VAR:
			case FIX_ARRAY:
				log_info("Casting %s to string", start);
				P = burp_insert(&g_new, start, "str(");
				if (got == FIX_ARRAY) {
					*P = 'S';
					g_translate.m_function.m_str = TRUE;
				}
				burpc(&g_new, ')');
				got = FIX_STRING;
				break;
			}
			break;
		case FIX_ARRAY:
			if (got != FIX_ARRAY) {
				log_info("Casting %s to array", start);
				g_translate.m_function.m_array = TRUE;
				P = burp_insert(&g_new, start, "Array(");
				burps(&g_new, ")");
				got = FIX_ARRAY;
			}
			break;
		}
		*gotP = got;
	}

	log_return();
	return endP;
}

// emitSpecial(): Just a wrapper for emitSpecial1()
// Returns 0 if not a special

static char * emitSpecial(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*endP;
	fix_typeE 	got;
	int			start;

	burpc(&g_new, START_EXPAND);
	endP  = emitSpecial1(startP, in_quotes, want, gotP);
	burpc(&g_new, END_EXPAND);
	return endP;
}

static fix_typeE combine_types(int offset, fix_typeE want_type, fix_typeE was_type, fix_typeE new_type)
{
	char	*P;

	log_enter("combine_types (offset=%d, want=%t, was=%t, new=%t)",
				offset, want_type, was_type, new_type);

	switch (want_type) {
	case FIX_ARRAY:
		if (new_type != FIX_ARRAY) {
			g_translate.m_function.m_array = TRUE;
			P = burp_insert(&g_new, offset, "Array(");
			burpc(&g_new, ')');
			new_type = FIX_ARRAY;
		}
		break;
	case FIX_STRING:
		if (new_type != FIX_STRING) {
			P = burp_insert(&g_new, offset, "str(");
			if (was_type == FIX_ARRAY) {
				*P = 'S';
				g_translate.m_function.m_str = TRUE;
			}
			burpc(&g_new, ')');
			new_type = FIX_STRING;
		}
		break;
	case FIX_INT:
		if (new_type != FIX_INT) {
			if (was_type == FIX_NONE) {
				P = burp_insert(&g_new, offset, "int(");
				new_type = FIX_INT;
			} else {
				P = burp_insert(&g_new, offset, "str(");
				if (was_type == FIX_ARRAY) {
					*P = 'S';
					g_translate.m_function.m_str = TRUE;
				}
				new_type = FIX_STRING;
			}
			burpc(&g_new, ')');
		}
		break;
	case FIX_VAR:
		// Return whatever we get
		break;
	}

	if (was_type == FIX_NONE) {
		// Do nothing more
		log_return_msg("Simple cast");
		return new_type;
	}
	if (was_type == new_type) {
		switch (was_type) {
		case FIX_ARRAY:
		case FIX_STRING:
			// Join two things together
			P = burp_insert(&g_new, offset, "+");
			log_return_msg("Concatenated");
			return new_type;
	}	}

	// Make them both strings and combine them
	if (new_type != FIX_STRING) {
		P = burp_insert(&g_new, offset, "+str(");
		if (new_type == FIX_ARRAY) {
			P[1] = 'S';
			g_translate.m_function.m_str = TRUE;
		}
		burpc(&g_new, ')');
	} else {
		P = burp_insert(&g_new, offset, "+");
	}

	if (was_type != FIX_STRING) {
		P = burp_insert(&g_new, offset, ")");
		P = burp_insert(&g_new, 0, "str(");
		if (was_type == FIX_ARRAY) {
			*P = 'S';
			g_translate.m_function.m_str = TRUE;
	}	}
	log_return_msg("Cast to strings and concatenated");
	return FIX_STRING;
}

// substitute(): Opens the door to the emit...() family of string transformations
// which can call each other in all kinds of ways to fully pythonize a bash value.
// Compare to only_expand() which does the same but in a more restricted way
// for FIX_EXPRESSIONs.

/* Convert double quotes so that all non-escaped bracketting double quotes
 * are given clear internal codes  to simplify subsequent logic 
 * Recursively expand expressions as needed
 *
 * We can assume we are working with something considered a bash word
 *
 * Returns the type of the result
 */

static fix_typeE substitute(fix_typeE want)
{
	fix_typeE	got;	// What I had
	fix_typeE	got1;	// What I'm now seeing
	fix_typeE	want1;
	int 		i,  in_quotes, quoted, c, c1, c2, offset;
	char		**arrayPP, *P, *P1, *P2;
	_BOOL      	is_file_expansion, quote_removal;
	_BOOL		is_outside_quotes;

	log_enter("substitute (want=%t)", want);

	switch (want) {
	case FIX_INT:
	case FIX_VAR:
		if (P = isInteger(g_buffer.m_P)) {
			burps(&g_new, P);
			return FIX_INT;
		}
	}

	is_outside_quotes = TRUE;
	is_file_expansion = FALSE;
	offset           = g_new.m_lth;

	for (P = g_buffer.m_P; c = *P ; ++P) {

		switch (c) {
		case '*':
		case '?':
		case '[':
		    // Any unprotected globbing character should trigger file globbing
			if (is_outside_quotes) is_file_expansion = TRUE;
			break;
		case '~':
		case '$':
		case '`':
		    // We have a "special" character
			//TODO:  Very bad!!!  quoted is not initialized! Compare to other invocations.
			P1 = emitSpecial(P, quoted, want, &got1);
			if (P1 && P1 != P) {
				P   = --P1;
				continue;
			}
			// Restore to where we were
			g_new.m_lth = offset;
			g_new.m_P[offset] = '\0';
			break;
		case '"':
			is_outside_quotes = !is_outside_quotes;
			break;
		case '\\':
		    // Skip/protect escaped characters
			if (P[1]) {
				++P;
	}	}	}
	g_new.m_lth = offset;

	got              = FIX_NONE;
	in_quotes        = -1;
	quote_removal    = FALSE;
	want1            = (is_file_expansion ? FIX_STRING : want);

	for (P = g_buffer.m_P; ; ++P) {

		c = *P;

		if (c == '"') {
			// Quote removal
			quote_removal = TRUE;
			continue;
		}

		switch (c) {
		case '\0':
		case '~':
		case '$':
		case '`':
			// We have a "special" character or the null terminator.
			if (in_quotes < 0) {
				quoted = quote_removal;
			} else {
				burpc(&g_new, END_QUOTE);
				got1 = FIX_STRING;
				got = combine_types(in_quotes, want1, got, got1);
				in_quotes = -1;
				quoted = 1;
			}
			if (!c) {
				goto done;
			}
			quote_removal = FALSE;
			offset = g_new.m_lth;
			P1 = emitSpecial(P, quoted, want1, &got1);
			if (P1 && P1 != P) {
				got = combine_types(offset, want1, got, got1);
				P   = --P1;
				continue;
			}
			// Restore to where we were
			g_new.m_lth = offset;
			g_new.m_P[offset] = '\0';
		}

		quote_removal = FALSE;

		// Saw some normal character
		if (in_quotes < 0) {
			in_quotes = g_new.m_lth;
			burpc(&g_new, START_QUOTE);
		}
		if (c == '\\') {
			if (P[1]) {
				burpc(&g_new, '\\');
				c = *++P;
		}	}
		burpc(&g_new, c);
	}
done:

	if (is_file_expansion) {
		g_translate.m_uses.m_glob = TRUE;
		burp_insert(&g_new, 0, "Glob(");
		g_translate.m_function.m_glob = TRUE;
		burpc(&g_new, ')');
		got = FIX_ARRAY;
	}

	// Arrays always remain arrays
	// Vars can return anything

	switch (want) {
	case FIX_INT:
		if (got != FIX_INT) {
			P = burp_insert(&g_new, 0, "int(");
			burpc(&g_new, ')');
			got = FIX_INT;
		}
		break;
	case FIX_STRING:
		if (got != FIX_STRING) {
			P = burp_insert(&g_new, 0, "str(");
			if (got == FIX_ARRAY) {
				*P = 'S';
				g_translate.m_function.m_str = TRUE;
			}
			burpc(&g_new, ')');
			got = FIX_STRING;
		}
		break;
	}

	log_return();
	return got;
}

// Alternative to substitute() when we can't do any translation
// because we will later be calling translate_expression

static void only_expand(void)
{
	int 		i, c, offset;
	char		*P, *P1;
	fix_typeE	got1;
	_BOOL		is_quoted;

	if (P = isInteger(g_buffer.m_P)) {
		burps(&g_new, P);
		return;
	}

	is_quoted = FALSE;
	for (P = g_buffer.m_P; c = *P; ++P) {
		offset = g_new.m_lth;
		P1 = emitSpecial(P, is_quoted, FIX_EXPRESSION, &got1);
		if (P1 && P1 != P) {
			P   = --P1;
			continue;
		}
		// Restore to where we were
		g_new.m_lth = offset;
		g_new.m_P[offset] = '\0';

		if (c == '"') {
			is_quoted = !is_quoted;
			// Quote removal
			continue;
		}
		if (c == '\\') {
			if (P[1]) {
				burpc(&g_new, '\\');
				c = *++P;
		}	}
		burpc(&g_new, c);
	}
	return;
}

/* Avoid using keywords with special values in python but not in bash */

static void rename_special(void)
{
	int 	c, c1, lth, in_word;
	char	*P, *P1;

	in_word     = FALSE;
	lth         = 0;
	for (P = g_buffer.m_P; c = *P; ++P) {
		if (!in_word) {
			switch (c) {
			case '_':
				if (!strncmp(P, "__debug__", 9)) {
					lth = 9;
				}
				break;
			case 'c':
				if (!strncmp(P, "copyright", 9)) {
					lth = 9;
					break;
				}
				if (!strncmp(P, "credits", 7)) {
					lth = 7;
				}
				break;
			case 'E':
				if (!strncmp(P, "Ellipsis", 8)) {
					lth = 8;
				}
				break;
			case 'F':
				if (!strncmp(P, "False", 5)) {
					lth = 5;
				}
				break;
			case 'l':
				if (!strncmp(P, "license", 7)) {
					lth = 7;
				}
				break;
			case 'N':
				if (!strncmp(P, "None", 4)) {
					lth = 4;
					break;
				} 
				if (!strncmp(P, "NotImplemented", 14)) {
					lth = 14;
				}
				break;
			case 'T':
				if (!strncmp(P, "True", 4)) {
					lth = 4;
				}
				break;
			}
			if (!lth) {
				goto ignore;
			}
			switch (P[lth]) {
			case '\0':
			case '\n':
			case '\t':
			case ' ':
			case ')':
			case '(':
			case '=':
			case START_QUOTE:
				break;
			default:
				lth = 0;
				goto ignore;
			}
			for (P1 = P; c1 = *P1; ++P1) {
				if (c1 >= 'a' && c1 <= 'z') {
					*P1 += 'A' - 'a';
					break;
				}
				if (c1 >= 'A' && c1 <= 'Z') {
					*P1 += 'a' - 'A';
					break;
			}	}
			lth = 0;
		}
ignore:
		switch (c) {
		case '\n':
		case '\t':
		case ' ':
		case END_QUOTE:
		case '[':
		case '(':
		case ')':
			in_word = FALSE;
			break;
		case START_EXPAND:
			P = endExpand(P) - 1;
		default:
			in_word = TRUE;
		}
	}
}


static void compactWhiteSpace(void)
{
	char *P, *P1, *P3;
	int	 separation;

	for (P = g_buffer.m_P; P1 = strchr(P, END_QUOTE); P = P1+1) {
		for (P3 = P1+1; isspace(*P3); ++P3);
		if (!*P3) {
			break;
		}
		if (*P3 == START_QUOTE && (separation = (P3 - P1)) > 2) {
			// Join two adjacent strings with a blank
			memcpy(P1+2, P3, g_buffer.m_lth - (P3 - g_buffer.m_P));
			g_buffer.m_lth -= (separation - 2);
			g_buffer.m_P[g_buffer.m_lth] = '\0';
		}
	}
}

static void unmarkQuotes(_BOOL delete_quotes)
{
	char	*P, *P1;
	int		c, expand_depth;

	expand_depth = 0;
	for (P = P1 = g_buffer.m_P; c = *P; ++P) {
		switch (c) {
		case START_EXPAND:
			++expand_depth;
			break;
		case END_EXPAND:
			--expand_depth;
			break;
		case END_QUOTE:
			if (P[1] == START_QUOTE) {
				++P;
				continue;
			}
		case START_QUOTE:
			if (delete_quotes && !expand_depth) {
				continue;
			}
			c = '"';
		}
		*P1++ = c;
	}
	*P1 = '\0';
	g_buffer.m_lth = P1 - g_buffer.m_P;
}

static void unmarkExpand(void)
{
	char	*P, *P1;
	int		c;

	for (P = P1 = g_buffer.m_P; c = *P; ++P) {
		switch (c) {
		case START_EXPAND:
		case END_EXPAND:
			continue;
		}
		*P1++ = c;
	}	
	*P1 = '\0';
	g_buffer.m_lth = P1 - g_buffer.m_P;
}

static void unescapeDollar(void)
{
	char *P, *P1;

	for (P1 = P = g_buffer.m_P; *P; ++P) {
		if (*P == '\\' && P[1]) {
			if (P[1] == '$') {
				continue;
			}
			*P1++ = *P++;
		}
		*P1++ = *P;
	}
	g_buffer.m_lth = P1 - g_buffer.m_P;
	*P1 = '\0';
	return;
}
	
/* Transforms g_buffer
 * Separator can be:
	 + indicating concatonate parts (normal behaviour)
	 " indicating we are embedding the contents of this string inside popen
 */

// fix_string1(): Topmost filter function after fixBraced() has peeled off any
// brace expressions, that is,  "{1..10}"  and  "foo{a,bbb,cc}bar"  stuff
static char * fix_string1(fix_typeE want, fix_typeE *gotP)
{
	fix_typeE	got;
	_BOOL		is_expression;

	log_enter("fix_string1 (want=%t)", want);

	got = FIX_NONE;
	if (!g_buffer.m_lth) {
		goto done;
	}
		
	replaceSingleQuotes(); // change quoting inside g_buffer. Uses g_new as scratch.

	// Clean up g_new before reusing it
	burp_close(&g_new);

	if (want != FIX_EXPRESSION) {
		is_expression           = FALSE;
		g_dollar_expr_nesting_level = 0;
		got = substitute(want);	
	} else {
		is_expression           = TRUE;
		g_dollar_expr_nesting_level = 1;
		only_expand();
	}

	// Everything has been written to g_new
	// Make it look as if it never left g_buffer
	exchange_burp_buffers(&g_buffer, &g_new);

	compactWhiteSpace();
	rename_special();
finish:
	unmarkQuotes(is_expression);
	unescapeDollar();
	if (is_expression) {
		char 		*translationP;

		// Don't allow array
		if (translate_expression(g_buffer.m_P, &translationP, FALSE)) {
			got         = FIX_EXPRESSION;
			g_new.m_lth = 0;
			burps(&g_new, translationP);
			exchange_burp_buffers(&g_buffer, &g_new);
		}
	}
	unmarkExpand();
done:
	if (gotP) {
		*gotP = got;
	}

	log_return();
	return g_buffer.m_P;
}


// fixBracedString(): The topmost filtering function under fix_string().
// Processes brace expressions like {1..10} and foo{a,bbb,cc}bar but NOT
// dollar-brace expressions like ${x}. Notice how the code looks for '.' or ',' inside {}.

static char * fixBracedString(const char *startP, fix_typeE want, fix_typeE *gotP)
{
	extern char **brace_expand(char *textP);   // from braces.c in the bash API

	const char *P;

	char	**arrayPP;
	int		c, in_quotes, state;
	char	*resultP;

	log_enter("fixBracedString (startP=%q, want=%t)", startP, want);
log_deactivate();

	if (want == FIX_EXPRESSION) {
		goto pass_through;
	}
	in_quotes = 0;
	state     = 0;
	for (P = startP; c = *P; ++P) {
		if (in_quotes) {
			switch (c) {
			case '\'':
			case '"':
				if (in_quotes == c) {
					in_quotes = 0;
				}
				continue;
			case '\\':
				if (in_quotes != '\'' && P[1]) {
					// Skip over next character
					++P;
			}	}
			continue;
		} 

		switch (c) {
		case '\'':
		case '"':
			in_quotes = c;
			continue;
		case '\\':
			if (P[1]) {
				// Skip over next character
				++P;
			}
			continue;
		case '$':
			if (P[1] == '{') {
				// Skip dollar-brace constructs
				++P;
			}
			continue;
		case '{':
			state = 1;
			continue;
		case ',':
			if (state == 1) {
				state = 2;
			}
			continue;
		case '.':
			if (state == 1 && P[1] == '.') {
				state = 2;
			}
			continue;
		case '}':
			if (state == 2) {
				// End of brace expr
				goto expand_brace_expr;
			}
			state = 0;
		}
	}
	goto pass_through;

expand_brace_expr:
	resultP = NULL;
	arrayPP = brace_expand((char *) startP);   // calling bash API here
	if (arrayPP) {
		if (arrayPP[0]) {
			fix_typeE	single_want_type;
			int			i;
			_BOOL		want_array;
	
			want_array = (want == FIX_ARRAY);
			g_braced.m_lth = 0;
			if (want_array) {
				single_want_type = FIX_VAR;
				burpc(&g_braced, '[');
			} else {
				single_want_type = want;
			}
	
			for (i = 0; P = arrayPP[i]; ) {
				string_to_buffer(arrayPP[i]);
				P = fix_string1(single_want_type, gotP);
				burps(&g_braced, P);
				++i;
				if (arrayPP[i]) {
					burpc(&g_braced,',');
			}	}
			if (want_array) {
				burpc(&g_braced, ']');
				*gotP = want;
			}
			resultP = g_braced.m_P;
		}
		free(arrayPP);
		if (resultP) {
log_activate();
		    log_return_msg("brace expression processed");
			return resultP;
	}	}

pass_through:
	string_to_buffer(startP);
	resultP = fix_string1(want, gotP);

log_activate();
	log_return_msg("Brace expr not detected, pass through to emitSpecial()");
	return resultP;
}

// The top-level API for converting bash value expressions to python.
// Basically wraps fixBracedString(), which wraps ...
char * fix_string(const char *stringP, fix_typeE want, fix_typeE *gotP)
{
	char *P;

	int save;
	if (want == FIX_NONE || !*stringP) {
		return (char *) stringP;
	}

	save             = g_translate_html;
	g_translate_html        = 0;
	g_dollar_expr_nesting_level = 0;

	P = fixBracedString(stringP, want, gotP);

	g_translate_html = save;
	g_dollar_expr_nesting_level = 0;

	return P;
}

#if defined(TEST) && !defined(SKIPMAIN)
int main(int argc, char **argv)
{
	char		*bufferP   = 0;
	size_t		buffer_lth = 0;
	char		*P;
	int			lth;
	fix_typeE	got;

	while(0 <= getline(&bufferP, &buffer_lth, stdin)) {
		lth = strlen(bufferP);
		if (lth && bufferP[lth-1] == '\n') {
			bufferP[lth-1] = 0;
		}
		printf("> %s$\n", bufferP);
		P = fix_string(bufferP, FIX_STRING, &got);
		printf("< %s$\n", P);
	}
	return(0);
}
#endif

#endif
