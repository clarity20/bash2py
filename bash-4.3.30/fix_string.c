#include "config.h"

#ifdef BASH2PY

#include <stdio.h>
#include "bashansi.h"
#include <ctype.h>
#include <assert.h>
#include "burp.h"
#include "fix_string.h"

translateT	g_translate = {0};

static burpT g_buffer      = {0, 0, 0};
static burpT g_new         = {0, 0, 0};
static burpT g_braced      = {0, 0, 0};

int	g_inside_function = 0;
int g_function_parms  = 0;

int g_rc_identifier   = 0;

static int g_underDollarExpression = 0;

extern int  		g_translate_html;

#ifndef TEST
extern	void seen_global(const char *nameP, int local);
#endif

static void
exchange(burpT *oldP, burpT *newP)
{
	burpT	temp;

	temp   = *oldP;
	*oldP  = *newP;
	*newP  = temp;
}

char *
endQuotedString(char *stringP)
{
	char	*P;
	int		c;

	switch (*stringP) {
	case START_QUOTE:
	{
		int in_string = 1;
		for (P = stringP + 1; ; ++P) {
			switch (c = *P) {
			case 0:
				return 0;
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
			case 0:
				return 0;
			case '"':
				return P;
			case '\\':
				if (P[1]) {
					++P;
		}	}	}
	case '\'':
		for (P = stringP + 1; ; ++P) {
			switch (c = *P) {
			case 0:
				return 0;
			case '\'':
				return P;
	}	}	}
	return 0;
}

char *
endArray(char *startP)
{
	char	*P, *endP;
	int		c, in_array;

	
	c = *startP;
	if (c != '[') {
		return 0;
	}
	in_array = 1;
	for (P = startP + 1; ; ++P) {
		switch (c = *P) {
		case 0:
			return 0;
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
				return 0;
			}
			P = endP;
			continue;
		case '[':
			++in_array;
		default:
			continue;
}	}	}

/* Returns pointer to character after end of expansion */

char *
endExpand(char *startP)
{
	char *endP;
	int	 expand;

	assert(*startP == START_EXPAND);
	expand = 1;
	for (endP = startP+1; expand; ++endP) {
		switch(*endP) {
		case 0:
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

static int
read_hex(char **PP, int max_digits)
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
	
static void
emit_hex(int c)
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

char *
isInteger(char *startP)
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
			return 0;
		}
		*P1++ = c;
	}
	*P1 = 0;
		
	base = 10;
	P    = temp;
	switch (*P) {
	case 0:
		return 0;
	case '-':
	case '+':
		if (!P[1]) {
			return 0;
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
				return 0;
			}
			value = read_hex(&P, sizeof(temp));
			if (*P) {
				return 0;
			}
			return temp;
		default:
			base = 8;
			break;
	}	}

	for (; ; ++P) {
		switch (c = *P) {
		case 0:
			return temp;
		case '9':
		case '8':
			if (base < 10) {
				return 0;
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
		return 0;
	}
}

/* Initialise starting g_buffer with content of stringP */

static void
string_to_buffer(const char *stringP)
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

static void
replaceSingleQuotes(void)
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
				in_quotes = c;
			} else if (in_quotes == c) {
				in_quotes = 0;
			} else {
				burpc(&g_new, '\\');
			}
			break;
		case '\'':
			if (!in_quotes) {
				in_quotes = c;
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
				burpc(&g_new, '\\');
				burpc(&g_new, 'n');
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
	exchange(&g_buffer, &g_new);
}

static void
emitQuotedString(char *startP)
{
	char *P;
	int	 c;
	
	burpc(&g_new, '"');
	for (P = startP; ; ++P) {
		c = *P;
		switch (c) {
		case 0:
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
}	}

static char *emitSpecial1(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP);

static char *
emitSimpleVariable(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P, *endP;
	int			c;
	fix_typeE	got;

	got = FIX_NONE;

	P = startP;
	switch (c = *P++) {
	case '*':
		g_translate.m_expand.m_star = 1;
		burp(&g_new,"Expand.star(%d)", in_quotes);
		got = (in_quotes ? FIX_STRING : FIX_ARRAY);
		break;
	case '@':
		g_translate.m_expand.m_at = 1;
		burp(&g_new,"Expand.at()");
		got = FIX_ARRAY;
		break;
	case '#':
		// Expands to the number of positional parameters
		g_translate.m_expand.m_hash = 1;
		burps(&g_new, "Expand.hash()");
		got = FIX_INT;
		break;
	case '$':
		// Expands to the process id of the shell
		g_translate.m_expand.m_dollar = 1;
		burps(&g_new, "Expand.dollar()");
		got = FIX_INT;
		break;
	case '!':
		// Expands to the process ID of the background (asynchronous) command
		g_translate.m_expand.m_exclamation = 1;
		burps(&g_new, "Expand.exclamation()");
		got = FIX_INT;
		break;
	case '_':
		g_translate.m_expand.m_underbar = 1;
		burps(&g_new, "Expand.underbar()");
		got = FIX_STRING;
		break;
	case '-':
		// Expands to the current option flags
		g_translate.m_expand.m_hyphen = 1;
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
		if (g_inside_function) {
			int 	parameter;

			parameter = c - '0';
			burp(&g_new, "_p%d", parameter);
			if (g_function_parms < parameter) {
				g_function_parms = parameter;
			}
		} else {
			g_translate.m_uses.m_sys = 1;
			burps(&g_new, "sys.argv[");
			burpc(&g_new, c);
			burps(&g_new, "]");
		}
		got = FIX_VAR;
		break;
	case '?':
		burp(&g_new, "_rc%d", g_rc_identifier);
		got = FIX_INT;
		break;
	default:
		if (c != '_' && !isalpha(c)) {
			return 0;
		}
		burpc(&g_new, c);
		for (; (c = *P) && (c == '_' || isalnum(c)); ++P) {
			burpc(&g_new, c);
		}
		if (!g_underDollarExpression) {
			burps(&g_new, ".val");
		}
		got = FIX_VAR;
#ifndef TEST
		*P = 0;
        seen_global(startP, 0);
		*P = c;
#endif
	}
	*gotP = got;
	return P;
}
	
static char *
emitString(char *startP, const char *terminatorsP, int under_quotes)
{
	char 		*P, *endP;
	int			c, offset, in_quotes;
	fix_typeE	got;

	in_quotes = 0;
	for (P = startP; ; ++P) {
		c = *P;
		offset = g_new.m_lth;
		if (in_quotes) {
			burpc(&g_new, '"');
		}
		if (P != startP) {
			burpc(&g_new, '+');	
		}
		endP = emitSpecial1(P, under_quotes, FIX_STRING, &got);
		if (endP) {
			in_quotes = 0;
			P = endP - 1;
			continue;
		}
		// Undo emission
		g_new.m_lth = offset;
		if (!c) {
			return 0;
		}
		if (strchr(terminatorsP, c)) {
			goto done;
		}
		if (!in_quotes) {
			if (P != startP) {
				burpc(&g_new, '+');
			}
			burpc(&g_new, '"');
			in_quotes = 1;
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
	if (in_quotes) {
		burpc(&g_new, '"');
	}
	return P;
}

static char *
emitFunction(char *nameP, char *parm1P, char *parm2P, int indirect, int under_quotes)
{
	char 		*endP;
	int	 		offset;

	burps(&g_new, nameP);
	burpc(&g_new, '(');
	if (indirect) {
		g_translate.m_function.m_get_value = 1;
		burp(&g_new, "GetValue(%s.val)", parm1P);
	} else {
		emitQuotedString(parm1P);
	}
	if (parm2P) {
		offset = g_new.m_lth;
		burpc(&g_new, ',');
		endP = emitString(parm2P, "[}", under_quotes);
		if (!endP) {
			return 0;
		}
		if (endP == parm2P) {
			g_new.m_lth = offset;
	}	}
	burpc(&g_new, ')');
	return endP;
}

static char *
emitVariable(char *startP, int braced, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P, *endP, *functionP, *end_start1P, *start2P;
	int			c, array, indirect;
	fix_typeE	got;

	start2P     = 0;
	functionP   = 0;

	*gotP = FIX_STRING;

	if (braced) {

		indirect = (*startP == '!');
		if (indirect) {
			indirect = 1;
			++startP;
		}
		for (start2P = startP; ; ++start2P) {
			end_start1P = start2P;
			switch(c = *start2P) {
			case 0:
				return 0;
			case '*':
				if (indirect == 1 && start2P[1] == '}') { 	// ${!prefix*}
					g_translate.m_expand.m_prefixStar = 1;
					functionP = "Expand.prefixStar";
					indirect  = 0;
				}
				break;
			case '@': 					// ${!prefix@}
				if (indirect == 1 && start2P[1] == '}') {
					g_translate.m_expand.m_prefixAt = 1;
					functionP = "Expand.prefixAt";
					indirect  = 0;
				}
				break;
				

			case '[':	// Subscript
				if (indirect == 1) {
					if (!strncmp(start2P, "[*]}", 4)) {	// ${!name[*]}
						g_translate.m_expand.m_indicesStar = 1;
						functionP = "Expand.indicesStar";
						indirect  = 0;
						start2P  += 2;
					}
					if (!strncmp(start2P, "[@]}", 4)) {	// ${!name[@]}
						g_translate.m_expand.m_indicesAt = 1;
						functionP = "Expand.indicesAt";
						indirect  = 0;
						start2P  += 2;
					}
				} 
			case '}':	// End of expansion
				break;
			case '-':
				g_translate.m_expand.m_minus = 1;
				functionP = "Expand.minus";
				break;
			case '=':
				g_translate.m_expand.m_eq = 1;
				functionP = "Expand.eq";
				break;
			case '?':
				g_translate.m_expand.m_qmark = 1;
				functionP = "Expand.qmark";
				break;
			case '+':
				g_translate.m_expand.m_plus = 1;
				functionP = "Expand.plus";
				break;
			case ':':
				switch (*++start2P) {
				case '-':
					g_translate.m_expand.m_colon_minus = 1;
					functionP = "Expand.colonMinus";
					break;
				case '=':
					g_translate.m_expand.m_colon_eq = 1;
					functionP   = "Expand.colonEq";
					break;
				case '?':
					g_translate.m_expand.m_colon_qmark = 1;
					functionP   = "Expand.colonQmark";
					break;
				case '+':
					g_translate.m_expand.m_colon_plus = 1;
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
				if (indirect) {
					c = *end_start1P;
					*end_start1P = 0;
					burp(&g_new,"GetValue(%s.val)", startP);
				    *end_start1P = c;
					endP = end_start1P;
					goto done;
				}
			} else {
				assert(start2P);
				++start2P;
				c = *end_start1P;
				*end_start1P = 0;
				endP = emitFunction(functionP, startP, start2P, indirect, in_quotes);
				*end_start1P = c;
				if (!endP) {
					return 0;
				}
				goto done;
			}
			break;
	}	}
	endP = emitSimpleVariable(startP, in_quotes, want, gotP);
done:
    if (braced) {
		int offset;

		array = (*endP == '[');
		if (array) {
			*gotP = FIX_VAR;
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
			case 0:
				return 0;
			case '}':
				++endP;
				goto finish;
			case ']':
				if (array) {
					// We don't know what an array is of..
					burps(&g_new, "] ");
					array = 0;
				}
			}
			burpc(&g_new, c);
	}	}
finish:
	return endP;
}

/* $(( ... )) */

static char *
emitDollarExpression(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char 		*P, *P1, *endP;
	int  		start, offset, blank, c, brackets;
	fix_typeE	got;

	++g_underDollarExpression;
	*gotP    = FIX_INT;
	brackets = 0;
	blank    = 1;
	start    = g_new.m_lth;
	burpc(&g_new, '(');
	for (P = startP; (c = *P); ++P) {
		if (!isspace(c)) {
			blank = 0;
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
		case 0:
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
			++brackets;
			break;
		case ')':
			if (brackets) {
				--brackets;
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
	if (blank) {
		g_new.m_lth = start;
		burpc(&g_new, '0');
	} else {
		burpc(&g_new, ')');
#ifndef TEST
		// Only do for the outermost $((...))
		// Otherwise we will be translating Python output instead of the Bash
		if (g_underDollarExpression == 1) {
			// Do allow array
			if (!translate_expression(g_new.m_P + start, &P1, 1)) {
				return 0;
			}
			g_new.m_lth = start;
			burps(&g_new, P1);
		}
#endif
	}
	--g_underDollarExpression;
	return P;
} 

/* When the old-style backquote form of substitution is used, backslash retains its literal meaning except when followed by ‘$’, ‘`’, or ‘\’. The first backquote not preceded by a backslash terminates the command substitution. When using the $(command) form, all characters between the parentheses make up the command; none are treated specially. 
*/

static char *
emitCommand(char *startP, int new_style, int under_quotes, fix_typeE want, fix_typeE *gotP)
{
	int			old_underDollarExpression;
	char 		*endP;

	old_underDollarExpression = g_underDollarExpression;
	g_underDollarExpression   = 0;
	*gotP = FIX_STRING;
	burps(&g_new, "os.popen(");

	if (new_style) {
		endP = emitString(startP+2, ")", under_quotes);
	} else {
		endP = emitString(startP+1, "`", under_quotes);
	}
	if (endP) {
		++endP;
		g_translate.m_uses.m_os = 1;
		burps(&g_new, ").read().rstrip(\"\\n\")");
	}
	g_underDollarExpression = old_underDollarExpression;
	return endP;
}

static char *
emitTilde(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char 		*P, *endP;
	int			c, offset, in_string;
	fix_typeE	got;

	*gotP = FIX_STRING;
	burps(&g_new, "os.path.expanduser(\"~");
	in_string = 1;
	for (P = startP+1; ; ++P) {
		offset = g_new.m_lth;
		if (in_string) {
			burpc(&g_new, '"');
			in_string = 0;
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
			in_string = 1;
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

// Returns 0 if not special

static char *
emitSpecial1(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char *endP, *P;
	int	 c, c1, start;
	
	endP   = 0;
	start  = g_new.m_lth;

	switch (c = *startP) {
	case '~':
		endP = emitTilde(startP, in_quotes, want, gotP);
		if (gotP) {
			*gotP = FIX_STRING;
		}
		break;
	case '`':
		endP = emitCommand(startP, 0, in_quotes, want, gotP);
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
				endP = emitCommand(startP, 1, in_quotes, want, gotP);
				if (gotP) {
					*gotP = FIX_STRING;
			}	}
			break;
		case '{':
			endP = emitVariable(startP+2, 1, in_quotes, want, gotP);
			break;
		default:
			endP = emitVariable(startP+1, 0, in_quotes, want, gotP);
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
				P = burp_extend(&g_new, start, 4);
				memcpy(P, "str(", 4);
				if (got == FIX_ARRAY) {
					*P = 'S';
					g_translate.m_function.m_str = 1;
				}
				burpc(&g_new, ')');
				got = FIX_STRING;
				break;
			}
			break;
		case FIX_ARRAY:
			if (got != FIX_ARRAY) {
				P = burp_extend(&g_new, start, 6);	
				g_translate.m_function.m_array = 1;
				memcpy(P, "Array(", 6);
				burps(&g_new, ")");
				got = FIX_ARRAY;
			}
			break;
		}
		*gotP = got;
	}
	return endP;
}

// Returns 0 if not a special

static char *
emitSpecial(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*endP;
	fix_typeE 	got;
	int			start;

	burpc(&g_new, START_EXPAND);
	endP  = emitSpecial1(startP, in_quotes, want, gotP);
	burpc(&g_new, END_EXPAND);
	return endP;
}

static fix_typeE
combine_types(int offset, fix_typeE want_type, fix_typeE was_type, fix_typeE new_type)
{
	char	*P;

	switch (want_type) {
	case FIX_ARRAY:
		if (new_type != FIX_ARRAY) {
			P  = burp_extend(&g_new, offset, 6);
			g_translate.m_function.m_array = 1;
			memcpy(P, "Array(", 6);
			burpc(&g_new, ')');
			new_type = FIX_ARRAY;
		}
		break;
	case FIX_STRING:
		if (new_type != FIX_STRING) {
			P = burp_extend(&g_new, offset, 4);
			memcpy(P, "str(", 4);
			if (was_type == FIX_ARRAY) {
				*P = 'S';
				g_translate.m_function.m_str = 1;
			}
			burpc(&g_new, ')');
			new_type = FIX_STRING;
		}
		break;
	case FIX_INT:
		if (new_type != FIX_INT) {
			P = burp_extend(&g_new, offset, 4);
			if (was_type == FIX_NONE) {
				memcpy(P, "int(", 4);
				new_type = FIX_INT;
			} else {
				memcpy(P, "str(", 4);
				if (was_type == FIX_ARRAY) {
					*P = 'S';
					g_translate.m_function.m_str = 1;
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
		return new_type;
	}
	if (was_type == new_type) {
		switch (was_type) {
		case FIX_ARRAY:
		case FIX_STRING:
			// Join two things together
			P = burp_extend(&g_new, offset, 1);
			*P ='+';
			return new_type;
	}	}

	// Make them both strings and combine them
	if (new_type != FIX_STRING) {
		P = burp_extend(&g_new, offset, 5);
		memcpy(P, "+str(", 5);
		if (new_type == FIX_ARRAY) {
			P[1] = 'S';
			g_translate.m_function.m_str = 1;
		}
		burpc(&g_new, ')');
	} else {
		P = burp_extend(&g_new, offset, 1);
		*P = '+';
	}

	if (was_type != FIX_STRING) {
		P = burp_extend(&g_new, offset, 1);
		*P = ')';
		P = burp_extend(&g_new, 0, 4);
		memcpy(P, "str(", 4);
		if (was_type == FIX_ARRAY) {
			*P = 'S';
			g_translate.m_function.m_str = 1;
	}	}
	return FIX_STRING;
}

/* Convert double quotes so that all non-escaped bracketting double quotes
 * are given clear internal codes  to simplify subsequent logic 
 * Recursively expand expressions as needed
 *
 * We can assume we are working with something considered a bash word
 *
 * Returns the type of the result
 */

static fix_typeE
substitute(fix_typeE want)
{
	fix_typeE	got;	// What I had
	fix_typeE	got1;	// What I'm now seeing
	fix_typeE	want1;
	int 		i, outside_quotes, in_quotes, quoted, c, c1, c2, offset, quote_removal;
	char		**arrayPP, *P, *P1, *P2;
	int			fileExpansion;

	/* Return an array of strings; the brace expansion of TEXT.
	 * Documentation says this is done before anything else
	 */

	switch (want) {
	case FIX_INT:
	case FIX_VAR:
		if (P = isInteger(g_buffer.m_P)) {
			burps(&g_new, P);
			return FIX_INT;
		}
	}

	outside_quotes   = 1;
	fileExpansion    = 0;
	offset           = g_new.m_lth;

	for (P = g_buffer.m_P; c = *P ; ++P) {

		switch (c) {
		case '*':
		case '?':
		case '[':
			fileExpansion |= outside_quotes;
			break;
		case '~':
		case '$':
		case '`':
			P1 = emitSpecial(P, quoted, want, &got1);
			if (P1 && P1 != P) {
				P   = --P1;
				continue;
			}
			// Restore to where we were
			g_new.m_lth = offset;
			g_new.m_P[offset] = 0;
			break;
		case '"':
			outside_quotes = !outside_quotes;
			break;
		case '\\':
			if (P[1]) {
				++P;
	}	}	}
	g_new.m_lth = offset;

	got              = FIX_NONE;
	in_quotes        = -1;
	quote_removal    = 0;
	want1            = (fileExpansion ? FIX_STRING : want);

	for (P = g_buffer.m_P; ; ++P) {

		switch (c = *P) {
		case 0:
		case '~':
		case '$':
		case '`':
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
			quote_removal = 0;
			offset = g_new.m_lth;
			P1 = emitSpecial(P, quoted, want1, &got1);
			if (P1 && P1 != P) {
				got = combine_types(offset, want1, got, got1);
				P   = --P1;
				continue;
			}
			// Restore to where we were
			g_new.m_lth = offset;
			g_new.m_P[offset] = 0;
		}

		if (c == '"') {
			// Quote removal
			quote_removal = 1;
			continue;
		}
		quote_removal = 0;

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

	if (fileExpansion) {
		g_translate.m_uses.m_glob = 1;
		burp_extend(&g_new, 0, 5);
		g_translate.m_function.m_glob = 1;
		memcpy(g_new.m_P, "Glob(", 5);
		burpc(&g_new, ')');
		got = FIX_ARRAY;
	}

	// Arrays always remain arrays
	// Vars can return anything

	switch (want) {
	case FIX_INT:
		if (got != FIX_INT) {
			P = burp_extend(&g_new, 0, 4);
			memcpy(P, "int(", 4);
			burpc(&g_new, ')');
			got = FIX_INT;
		}
		break;
	case FIX_STRING:
		if (got != FIX_STRING) {
			P = burp_extend(&g_new, 0, 4);
			memcpy(P, "str(", 4);
			if (got == FIX_ARRAY) {
				*P = 'S';
				g_translate.m_function.m_str = 1;
			}
			burpc(&g_new, ')');
			got = FIX_STRING;
		}
		break;
	}
		
	return got;
}

// Can't do any translation because we will later be calling
// translate_expression

static void
only_expand(void)
{
	int 		i, quoted, c, offset;
	char		*P, *P1;
	fix_typeE	got1;

	/* Return an array of strings; the brace expansion of TEXT.
	 * Documentation says this is done before anything else
	 */

	if (P = isInteger(g_buffer.m_P)) {
		burps(&g_new, P);
		return;
	}

	quoted = 0;
	for (P = g_buffer.m_P; c = *P; ++P) {
		offset = g_new.m_lth;
		P1 = emitSpecial(P, quoted, FIX_EXPRESSION, &got1);
		if (P1 && P1 != P) {
			P   = --P1;
			continue;
		}
		// Restore to where we were
		g_new.m_lth = offset;
		g_new.m_P[offset] = 0;

		if (c == '"') {
			quoted = !quoted;
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

static void
rename_special(void)
{
	int 	in_quotes, c, c1, lth, in_word, expand;
	char	*P, *P1;

	in_quotes   = 0;
	in_word     = 0;
	lth         = 0;
	for (P = g_buffer.m_P; c = *P; ++P) {
		if (!in_quotes && !in_word) {
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
			case 0:
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
			in_word = 0;
			break;
		case START_EXPAND:
			P = endExpand(P) - 1;
		default:
			in_word = 1;
		}
	}
}


static void
compactWhiteSpace(void)
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
			g_buffer.m_P[g_buffer.m_lth] = 0;
		}
	}
}

static void
unmarkQuotes(int delete_quotes)
{
	char	*P, *P1;
	int		c, in_expand;

	in_expand = 0;
	for (P = P1 = g_buffer.m_P; c = *P; ++P) {
		switch (c) {
		case START_EXPAND:
			++in_expand;
			break;
		case END_EXPAND:
			--in_expand;
			break;
		case END_QUOTE:
			if (P[1] == START_QUOTE) {
				++P;
				continue;
			}
		case START_QUOTE:
			if (delete_quotes && !in_expand) {
				continue;
			}
			c = '"';
		}
		*P1++ = c;
	}	
    *P1 = 0;
    g_buffer.m_lth = P1 - g_buffer.m_P;
}

static void
unmarkExpand(void)
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
    *P1 = 0;
    g_buffer.m_lth = P1 - g_buffer.m_P;
}

static void
unescapeDollar(void)
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
	*P1 = 0;
	return;
}
	
/* Transforms g_buffer
 * Separator can be:
	 + indicating concatonate parts (normal behaviour)
     " indicating we are embedding the contents of this string inside popen
 */

static char *
fix_string1(fix_typeE want, fix_typeE *gotP)
{
	fix_typeE	got;
	int			is_expression;

	got = FIX_NONE;
    if (!g_buffer.m_lth) {
		goto done;
	}
		
	replaceSingleQuotes();	// Replace all usage of '..' by ".."

	// Nothing yet written to g_new
	g_new.m_lth  = 0;
	g_new.m_P[0] = 0;

	if (want != FIX_EXPRESSION) {
		is_expression           = 0;
    	g_underDollarExpression = 0;
		got = substitute(want);	
	} else {
		is_expression           = 1;
		g_underDollarExpression = 1;
		only_expand();
	}

	// Everything has been written to g_new
	// Make it look as if it never left g_buffer
	exchange(&g_buffer, &g_new);

	compactWhiteSpace();
	rename_special();			
finish:
	unmarkQuotes(is_expression);
	unescapeDollar();
	if (is_expression) {
		char 		*translationP;

		// Don't allow array
		if (translate_expression(g_buffer.m_P, &translationP, 0)) {
			got         = FIX_EXPRESSION;
			g_new.m_lth = 0;
			burps(&g_new, translationP);
			exchange(&g_buffer, &g_new);
		}
	}
	unmarkExpand();
done:
	if (gotP) {
		*gotP = got;
	}
	return g_buffer.m_P;
}

/* A correctly-formed brace expansion must contain unquoted opening and
   closing braces, and at least one unquoted comma or a valid sequence
   expression. Any incorrectly formed brace expansion is left unchanged. 
 */

static char *
fixBracedString(const char *startP, fix_typeE want, fix_typeE *gotP)
{
#ifndef TEST
	extern char **brace_expand(char *textP);

	const char *P;

	char	**arrayPP;
	int		c, in_quotes, state;
	char	*resultP;
	
	if (want == FIX_EXPRESSION) {
		goto dont_fire;
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
				goto fire;
			}
			state = 0;
		}
	}
	goto dont_fire;
fire:
	resultP = 0;
	arrayPP = brace_expand((char *) startP);
	if (arrayPP) {
		if (arrayPP[0]) {
			fix_typeE	want1;
			int			i, want_array, separator;
	
			want_array = (want == FIX_ARRAY);
			g_braced.m_lth = 0;
			if (want_array) {
				want1     = FIX_VAR;
				burpc(&g_braced, '[');
			} else {
				want1     = want;
			}
	
			for (i = 0; P = arrayPP[i]; ) {
				string_to_buffer(arrayPP[i]);
				P = fix_string1(want1, gotP);
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
		xfree(arrayPP);
		if (resultP) {
			return resultP;
	}	}
dont_fire:
#endif

	string_to_buffer(startP);
	resultP = fix_string1(want, gotP);
	return resultP;
}

char *
fix_string(const char *stringP, fix_typeE want, fix_typeE *gotP)
{
	char *P;

#ifndef TEST
	int save;
#endif
	if (want == FIX_NONE || !*stringP) {
		return (char *) stringP;
	}

#ifndef TEST
	save             = g_translate_html;
#endif
	g_translate_html        = 0;
    g_underDollarExpression = 0;

	P = fixBracedString(stringP, want, gotP);

#ifndef TEST
	g_translate_html = save;
	g_underDollarExpression = 0;

#endif
	return P;
}

#if defined(TEST) && !defined(SKIPMAIN)
int
main(int argc, char **argv)
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
