#include "config.h"

#ifdef BASH2PY

#include <stdio.h>
#include "bashansi.h"
#include <ctype.h>
#include <assert.h>
#include "burp.h"
#include "fix_string.h"

// Borrowed from subst.c:
extern char *skiparith __P((char *, int));
extern int skip_to_delim __P((char *, int, char *, int));

translateT	g_translate;

burpT g_buffer = {0,0,0,0,0,0};
burpT g_new = {0,0,0,0,0,0};
burpT g_braced = {0,0,0,0,0,0};   // Only directly used in FixBraced()

_BOOL g_regmatch_special_case = FALSE;
_BOOL g_is_inside_function = FALSE;
int g_function_parms_count  = 0;
globConversionStateE g_globConversionState = INACTIVE;
int g_rc_identifier   = 0;

static int g_dollar_expr_nesting_level = 0;

extern	void seen_global(const char *nameP, _BOOL local);

static char * emitSpecialSubexpression(char *startP, fix_typeE want, fix_typeE *gotP);
static char * fixBracedString(const char *startP, fix_typeE want, fix_typeE *gotP);

char g_regmatch_var_name[] = "BASH_REMATCH";

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
	int		nesting_level;
	
	if (*startP != '[') {
		return NULL;
	}
	nesting_level = 1;
	for (P = startP + 1; ; ++P) {
		switch (*P) {
		case '\0':
			return NULL;
		case ']':
			if (--nesting_level) {
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
			++nesting_level;
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

// If a string denotes an integer (hex, octal, decimal strings with or without sign or quotes all okay) 
// then return a pointer to a "cleaner" version of the string, else return NULL.
// If the "cleaner" string has any non-numeric content, return a NULL, so this is NOT like atoi().

char * integerStringOrEmpty(char *inputString)
{
	static char workingString[32];
	char valid_digits[16];

	char *readerP, *writerP, *endP;
	int  base, c, num_valid;

	memset(workingString, '\0', sizeof(workingString));
	writerP = workingString;
	endP = workingString + sizeof(workingString) - 1;

	// Filter out quotes and backslashes by walking reader & writer ptrs along the string
	for (readerP = inputString; c = *readerP; ++readerP) {
		if (c=='"' || c==START_QUOTE || c==END_QUOTE)
			continue;

		if (c=='\\' && readerP[1])
			c = *++readerP;

		if (endP <= writerP) {
			log_info("integerStringOrEmpty: input string too long");
			return NULL;
		}

		*writerP++ = c;
	}
	*writerP = '\0';
		
	if (*workingString == '\0') {
		log_info("integerStringOrEmpty: Input string is empty");
		return NULL;
	}

	// Accept a leading + or - sign
	base = 10;
	readerP    = workingString;  // N.B. reseat reader to the working string
	if (strchr("+-", *readerP)) {
		if (!readerP[1]) {
			return NULL;
		}
		++readerP;
	}

	// Detect hex/octal indicators. Dispense with hex strings.
	if (*readerP == '0') {
		if (toupper(readerP[1]) == 'X') {   // hex
			readerP += 2;
			if (!*readerP) {
				return NULL;
			}
			read_hex(&readerP, sizeof(workingString));
			if (*readerP) {
				return NULL;
			}
			return workingString;
		}
		else {   // octal
			base = 8;
		}
	}

	// Scan the string and return it. Any non-digit causes NULL return.
	strcpy(valid_digits,"0123456789"); valid_digits[base] = '\0';
	num_valid = strspn(readerP, valid_digits);
	if (num_valid != strlen(readerP))
	{
//		log_info("integerStringOrEmpty: Invalid digit %c for base-%d number.", readerP[num_valid], base);
		return NULL;
	}
	return workingString;

}

/* Initialise starting g_buffer with content of stringP */

static void string_to_buffer(const char *stringP)
{
	const char	*P0;
	int			lth;

  	for (P0 = stringP; isspace(*P0); ++P0);
	burp_reset(&g_buffer);
	burps(&g_buffer, P0);
	for (lth = g_buffer.m_lth; lth && isspace(g_buffer.m_P[lth-1]); --lth);
  	g_buffer.m_P[lth] = 0;
	g_buffer.m_lth = lth;
}

/* Replace single quotes by double quotes escaping contents correctly
 * this simplifies things since no longer need to worry about single quote
 * Also handle ANSI-C Quoting translation (3.1.2.4)
 */

static int g_little_endian = -1;

// Convert any single quotes in g_buffer to escaped double quotes, using g_new as a temp.
static void replaceSingleQuotes(void)
{
	int 	in_quotes;
	unsigned char c, c1;
	char	*P;

	in_quotes   = 0;
	g_new.m_lth = 0;

	for (P = g_buffer.m_P; c = *((unsigned char *) P); ++P) {

		// The giant switch{...} manages state when special characters are seen
		// The final burpc() copies the character

		switch (c) {
		case '"':
			if (!in_quotes) {
				in_quotes = c;   // remember opening double quote
			} else if (in_quotes == c) {
				in_quotes = 0;   // found a terminating double quote
			} else {
				burpc(&g_new, '\\');    // escaped, dollared or strong-quoted double quote
			}
			break;
		case '\'':
			if (!in_quotes) {
				in_quotes = c;    // remember opening single quote
				c         = '"';  // but replace with double quote
			} else if (in_quotes == c) {
				in_quotes = 0;
				c         = '"';    // replace closing single with double
			} else if (in_quotes == '$') {
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
	swap_burps(&g_buffer, &g_new);
}

// emitQuoted(): Weak-quotes the supplied string into g_new
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
			log_return();
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
static char *emit_enclosed_subexpr(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP);

// emitSimpleVariable(): pythonifies nonbraced variables like $foo, $#, and $?, writing to g_new
static char * emitSimpleVariable(char *afterDollarP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P;
	int			c, len;
	fix_typeE	got;

	got = FIX_NONE;  // Logically guaranteed to change to not-NONE

	P = afterDollarP;
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
			// Invalid character at beginning of variable name
			return NULL;
		}
		len = strlen(g_regmatch_var_name);
		if (0 == strncmp(afterDollarP, g_regmatch_var_name, len)) {
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

// emitString(): Turns the input string into text suitable for print() by quoting
// the literal parts and concatenating them ('+') with the variable parts
static char * emitString(char *startP, const char *terminatorsP, int in_quotes)
{
	char 		*P, *endP;
	int			c, offset, is_inside_quotes;
	fix_typeE	got;

	log_enter("emitString (startP=%q, terminatorsP=%q, in_quotes=%d)",
			  startP, terminatorsP, in_quotes);
	is_inside_quotes = FALSE;

	// Build the output in pieces
	for (P = startP; ; ++P) {
		c = *P;
		offset = g_new.m_lth;
		if (is_inside_quotes) {
			burpc(&g_new, '"');
		}
		if (P != startP) {
			burpc(&g_new, '+');	
		}
		endP = emit_enclosed_subexpr(P, in_quotes, FIX_STRING, &got);
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

static char * emitFunction(char *functionName, char *parm1P, char *parm2P, _BOOL indirect, int in_quotes)
{
	char 		*endP;
	int	 		offset;

	log_enter("emitFunction (functionName=%q, parm1P=%q, parm2P=%q, indirect=%b, in_quotes=%d)",
			functionName, parm1P, parm2P, indirect, in_quotes);

	burp(&g_new, "%s(", functionName);
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

static char *findClosingBrace(char *buf)
{
	char *c = buf;
	int brace = 0;
	while (*c)
	{
		if (*c == '{' && *(c-1)!='\\') brace++;
		else if (*c == '}' && *(c-1)!='\\') {
			if(--brace == -1) {
				break;
			}
		}
		c++;
	}
	return c;
}

// emitVariable(): Process variable names following $ or ${.  After the closing } is seen,
// scan forward and recurse into emit_enclosed() to handle further special subexpressions.
// This function handles names having the indirection prefix "!" and/or a postfix in the set
//  [*] [@] :- := :+ :?  or one of the string operators  :  /  %  #
// while handing off simpler variable name instances to emitSimpleVariable.
// Any input text that follows } is then processed with nested calls to emitters as needed.
// The input is a pointer to g_buffer[2], and the final result is written to g_new.

static char * emitVariable(char *vblNameP, _BOOL is_variable_name_braced, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*P, *endP, *functionName, *afterVblName, *start2P;
	int			c, is_array;
	_BOOL		is_indirect;
	fix_typeE	got;
	int			is_colon_subrange = FALSE;

	log_enter("emitVariable (g_buffer[vblName]=%q, is_braced=%b, in_quotes=%d, want=%t)",
			vblNameP, is_variable_name_braced, in_quotes, want);
	start2P = NULL;
	functionName = NULL;

	*gotP = FIX_STRING;

	if (is_variable_name_braced) {

		is_indirect = (*vblNameP == '!');
		if (is_indirect) {
			++vblNameP;
		}
		for (start2P = vblNameP; ; ++start2P) {
			afterVblName = start2P;

			// (1) Use the symbol seen to choose a helper function
			switch(c = *start2P) {
			case '\0':
				log_return_msg("Early return, null character");
				return NULL;
			case '*':
				if (is_indirect == TRUE && start2P[1] == '}') { 	// ${!prefix*}
					g_translate.m_expand.m_prefixStar = TRUE;
					functionName = "Expand.prefixStar";
					is_indirect = FALSE;
				}
				break;
			case '@': 					// ${!prefix@}
				if (is_indirect == TRUE && start2P[1] == '}') {
					g_translate.m_expand.m_prefixAt = TRUE;
					functionName = "Expand.prefixAt";
					is_indirect = FALSE;
				}
				break;
				

			case '[':	// Subscript
				if (is_indirect == TRUE) {
					if (!strncmp(start2P, "[*]}", 4)) {	// ${!name[*]}
						g_translate.m_expand.m_indicesStar = TRUE;
						functionName = "Expand.indicesStar";
						is_indirect = FALSE;
						start2P  += 2;
					}
					if (!strncmp(start2P, "[@]}", 4)) {	// ${!name[@]}
						g_translate.m_expand.m_indicesAt = TRUE;
						functionName = "Expand.indicesAt";
						is_indirect = FALSE;
						start2P  += 2;
					}
				} 
			case '}':	// End of expansion
				break;
			case '-':
				g_translate.m_expand.m_minus = TRUE;
				functionName = "Expand.minus";
				break;
			case '=':
				g_translate.m_expand.m_eq = TRUE;
				functionName = "Expand.eq";
				break;
			case '?':
				g_translate.m_expand.m_qmark = TRUE;
				functionName = "Expand.qmark";
				break;
			case '+':
				g_translate.m_expand.m_plus = TRUE;
				functionName = "Expand.plus";
				break;
			case ':':
				switch (*++start2P) {
				case '-':
					g_translate.m_expand.m_colon_minus = TRUE;
					functionName = "Expand.colonMinus";
					break;
				case '=':
					g_translate.m_expand.m_colon_eq = TRUE;
					functionName   = "Expand.colonEq";
					break;
				case '?':
					g_translate.m_expand.m_colon_qmark = TRUE;
					functionName   = "Expand.colonQmark";
					break;
				case '+':
					g_translate.m_expand.m_colon_plus = TRUE;
					functionName = "Expand.colonPlus";
					break;
				default:
					// The character after : is none of the above.
					// Assume it's a subrange expr like ${x:y:z}
					is_colon_subrange = TRUE;
					--start2P;
				}
				break;
			case '/':
			case '%':
			case '#':
				{
				char *name, *value, *parameters, *pattern, *anchored_pattern, *repl, *replacement, *p;
				int delim, quoted=0, plen;
				char *rawflag;
				burpT bufferBackup, newBackup;
				_BOOL is_operator_doubled = FALSE;

				// Extract the name and the parameters: (1) pattern and (2) replacement ('/' operator only).
				// Much of the following code borrows from bash's parameter_brace_patsub ().
				memset(&bufferBackup, 0, sizeof(bufferBackup));
				memset(&newBackup, 0, sizeof(newBackup));
				value = start2P+1;

				if (*value == c)
				{ //TODO this needs to be generalized to handle /% and /% operators
					is_operator_doubled = TRUE;
					value++;
				}

				parameters = strdup(value);
				*(strrchr(parameters, '}')) = '\0';

				if (c == '/') 
				{
					delim = skip_to_delim (parameters, ((*value == '/') ? 1 : 0), "/", 0);
					if (parameters[delim] == '/')
					{
						parameters[delim] = 0;
						repl = parameters + delim + 1;
					}
					else
						repl = (char *)NULL;
					if (repl && *repl == '\0')
						repl = (char *)NULL;
				}

				// Expand the pattern, name and (if op='/') replacement and build the python around them.
				// We want fix_string() to do the dirty work, but this requires staging & unstaging
				// the global buffers carefully because fix_string can alter them too.
 
				got = FIX_STRING;
				// Back up the real buffers before running fix_string()
				swap_burps(&g_buffer, &bufferBackup);
				swap_burps(&g_new, &newBackup);
				// Expand & pythonify the pattern
				g_globConversionState = !(is_operator_doubled || c=='/') ? CONVERTING_UNGREEDY : CONVERTING;
				pattern = strdup(fix_string(parameters, FIX_STRING, &got));

				// Finish the pattern with raw marker and/or anchor if required
				plen = strlen(pattern);
				switch (c)
				{
					case '#':
						anchored_pattern = malloc(plen+6);
						if (*pattern == 'r') {   // Quoted-string patterns
							sprintf(anchored_pattern, "%.2s^%s", pattern, pattern+2);
						} else {  // Other patterns e.g. variable names
							sprintf(anchored_pattern, "r'^'+%s", pattern);
						}
						break;
					case '%':
						anchored_pattern = malloc(plen+6);
						p = pattern + plen-1;  // last char of pattern
						if (strchr("\"\'", p) != NULL) { //if (*pattern == 'r') {
							sprintf(anchored_pattern, "%s", pattern);
							sprintf(p, "$%c", *p);
						}
						else
							sprintf(anchored_pattern, "%s+r'$'", pattern);
						break;
					case '/':
						//TODO This is where to add logic for '/#' and '/%' operators
						anchored_pattern = pattern;
						break;
				}

				// Extract the name from vblName
				g_globConversionState = INACTIVE;
				// To guard against global memory issues, copy name into a new buffer
				// (1) Mark end of name in the original buffer
				*start2P = '\0';
				// (2) (TODO) Basically do "name = strdup(vblNameP)" as we expect name will not need
				// expanding. But for now, to make the python self-consistent we need to insert
				// the regrettable "str(XXX.val)" markup, We do this with 3 LOC:
				p = malloc(strlen(vblNameP)+2);
				sprintf(p, "$%s", vblNameP);  // '$' tricks fix_string() into treating name like a var name
				name = strdup(fix_string(p, FIX_STRING, &got));
				// (3) Unmark the end of vblName
				*start2P = c;

				// Expand the replacement
				replacement = strdup( (c=='/') ? fix_string(repl, FIX_STRING, &got) : "''");
				// Ensure the glob-to-regex conversion is only attempted once
				g_globConversionState = PROTECTING;
				// Restore the buffers
				swap_burps(&g_buffer, &bufferBackup);
				swap_burps(&g_new, &newBackup);

				// Construct the python code
				g_translate.m_uses.m_re = TRUE;
				burp(&g_new, "re.sub(%s, %s, %s", anchored_pattern, replacement, name);
				if (c=='/' && !is_operator_doubled)
					burps(&g_new, ", count=1");
				burpc(&g_new, ')');

				free(p);
				free(name);
				free(parameters);
				if (anchored_pattern != pattern) free(anchored_pattern);
				free(pattern);
				free(replacement);

				endP = strrchr(vblNameP, '}');
				goto done;
				}
				break;
			default:
				// Current character in buffer is nothing special
				continue;
			}

			// (2) Indirect references and string operations do not use functions
			if (!functionName) {
				if (is_indirect) {
					c = *afterVblName;
					*afterVblName = '\0';
					burp(&g_new,"GetValue(%s.val)", vblNameP);
					*afterVblName = c;
					endP = afterVblName;
					goto done;
				}
				else if (is_colon_subrange) {
					//TODO Any extra considerations for array-type variables?
					char *parameter1P, *parameter2P, *ogParameter1P;
					char *intP1, *intP2;
					int int1, int2;
					char tmp[32];
					burpT g_scratch0 = {0,0,0,0,0,0};
					burpT g_scratch = {0,0,0,0,0,0};
					burpT bufferBackup = {0,0,0,0,0,0};
					is_colon_subrange = FALSE;

					// Delimit the first parameter
					parameter1P = start2P+1;			// go past ':' to the first parameter
					//if (*parameter1P == ' ') parameter1P++;
					if (*parameter1P == ':') {		  // check for the form ${var::y}
						// Transform into ${var:0:y}
						burp_insert(&g_buffer, parameter1P-g_buffer.m_P, "0");
					}

					// Delimit the second parameter
					parameter2P = skiparith(parameter1P, ':');  // go to next ':' (or the end)
					if (*parameter2P == '\0')   // check for the form ${var:x} -- parameter2 is empty string
					{
						// Reseat both parameters in scratch buffers
						endP = strrchr(parameter1P, '}');
						assert(endP);
						burp(&g_scratch0, "%.*s", (int)(endP-parameter1P), parameter1P);
						burps(&g_scratch, parameter2P);
						parameter1P = g_scratch0.m_P;
						parameter2P = g_scratch.m_P;
					}
					else if (*parameter2P == ':') {		  // check for forms  ${var:x:y}  and  ${var:x:}
						// As above, reseat both parameters in scratch buffers
						parameter2P++;
						endP = findClosingBrace(parameter2P);   // forward search for closing '}'
						assert(endP);
						if (parameter2P == endP) {	 // the form ${var:x:}
							burp_insert(&g_buffer, (int)(parameter2P-g_buffer.m_P), "0");
							endP++;
						}
						burp(&g_scratch0, "%.*s", (int)(parameter2P-parameter1P)-1, parameter1P);
						burp(&g_scratch, "%.*s", (int)(endP-parameter2P), parameter2P);
						parameter1P = g_scratch0.m_P;
						parameter2P = g_scratch.m_P;
					}
					else goto done;		 // unexpected behavior from skiparith(). Malformed input?

					intP1 = parameter1P ? integerStringOrEmpty(parameter1P) : NULL;
					if (intP1) int1 = atoi(intP1);
					intP2 = parameter2P ? integerStringOrEmpty(parameter2P) : NULL;
					if (intP2) int2 = atoi(intP2);

					// Back up the original buffer & reseat its internal pointers
					burp(&bufferBackup, g_buffer.m_P);
					vblNameP = bufferBackup.m_P + (vblNameP-g_buffer.m_P);
					sprintf(tmp, ":%s", parameter1P);
					ogParameter1P = strstr(bufferBackup.m_P, tmp) + 1;

					/****  Construct the pythonic-style range expression ****/

					// (1) Set up the preamble
					burp(&g_new, "%.*s[", (ogParameter1P-vblNameP)-1, vblNameP);

					// (2) Set up the range, niceifying when there are numeric literals
					if (intP1 && intP2) {
						burp(&g_new, "%d:%d]", int1, (int2>0) ? int1+int2 : int2);
					}
					else if (intP1) {   // 7:   7:x+2
						burp(&g_new, "%d:%s]", int1, (parameter2P ? parameter2P : ""));
					}
					else if (intP2) {
						burp(&g_new, "%s:%d]", (parameter1P ? parameter1P : ""), int2);
					}
					else {   // Neither parameter is a numeric literal
						if (!parameter1P) {
							burp(&g_new, ":%s]", parameter2P);
						} else if (!parameter2P) {
							burp(&g_new, "%s:]", parameter1P);
						} else {
							burp(&g_new, "%s:%s+%s]", parameter1P, parameter1P, parameter2P);
						}
					}

					goto done;
				}   // is_colon_subrange
			}  // no helper function
			else {  // helper function
				assert(start2P);
				++start2P;
				c = *afterVblName;
				*afterVblName = '\0';
				endP = emitFunction(functionName, vblNameP, start2P, is_indirect, in_quotes);
				*afterVblName = c;
				if (!endP) {
					log_return_msg("Early return, null character");
					return NULL;
				}
				goto done;
			}
			break;
		}
	}

	// N.B. Because of gotos in the code above, this only runs for unbraced variables like $foo:
	endP = emitSimpleVariable(vblNameP, in_quotes, want, gotP);

done:
	if (is_variable_name_braced) {
		int offset;

		is_array = (*endP == '[' || g_regmatch_special_case);
		if (is_array) {
			*gotP = FIX_VAR;
			if (g_regmatch_special_case) {
				// Python gets the regmatch member by a function call () not a direct lookup []
				*endP = '(';
			}
		}

		// Attend to everything up to the closing '}'
		for (; ; ++endP) {
			c = *endP;
			offset = g_new.m_lth;
			P = emit_enclosed_subexpr(endP, in_quotes, want, &got);
			if (P) {
				endP = P - 1;
				continue;
			}
			g_new.m_lth = offset; // Undo emission
			switch (c) {
				case '\0':
					log_return_msg("Early return, null character");
					return NULL;
				case '}':
					++endP;
					goto finish;  // the single "right" way to end this loop
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
		}
	}
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
			endP = emit_enclosed_subexpr(P, in_quotes, FIX_INT, &got);
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
			P1 = translate_arithmetic_expr(g_new.m_P + start, TRUE);
			if (!P1) {
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

// When the backquote form of substitution is used, backslash retains its literal meaning
// except when followed by $, `, or \. The first backquote not preceded by a backslash
// terminates the command substitution. When using the $(command) form, all characters
// between the parentheses make up the command; none are treated specially.

static char * emitCommand(char *dollarOrTickP, _BOOL is_dollar_style, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	int			old_dollar_expr_nesting_level;
	char 		*endP;

	old_dollar_expr_nesting_level = g_dollar_expr_nesting_level;
	g_dollar_expr_nesting_level   = 0;
	*gotP = FIX_STRING;
	burps(&g_new, "os.popen(");

	if (is_dollar_style) {
		endP = emitString(dollarOrTickP+2, ")", in_quotes);
	} else {
		endP = emitString(dollarOrTickP+1, "`", in_quotes);
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
		endP = emit_enclosed_subexpr(P, in_quotes, FIX_STRING, &got);
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
			}
		}
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
// braced & unbraced variable exprs ${x} or $x , dispatches each to their appropriate handler,
// and attempts a type-fixing of the result. The results are written to g_new.

static char * emit_enclosed_subexpr(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char *endP, *P;
	int	 c, c1, start;
	
	log_enter("emit_enclosed_subexpr (startP=%q, in_quotes=%d, want=%t)", startP, in_quotes, want);

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

// emit_delimited(): A wrapper for emit_enclosed_subexpr()
// that delimits one subexpression of a compound sequence like  ${foo}"bar"$@"bu'z'z"`cat x.in`
// and is meant to be called repeatedly while traversing such a sequence.
// Writes the result to g_new and returns NULL if the input is not a special expression.
// Opens the door to the emit...() family of string transformations
// which can call each other in all kinds of ways to fully pythonize a bash value.

static char * emit_delimited(char *startP, int in_quotes, fix_typeE want, fix_typeE *gotP)
{
	char		*endP;
	fix_typeE 	got;
	int			start;

	burpc(&g_new, START_EXPAND);
	endP  = emit_enclosed_subexpr(startP, in_quotes, want, gotP);
	burpc(&g_new, END_EXPAND);
	return endP;
}

// combine_types(): Applies type modifier to the latest piece of g_new to make it compatible with its predecessor
// before joining them together
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
		// Do nothing more. e.g. we have only one component
		log_return_msg("Untyped value, will not cast");
		return new_type;
	}
	if (was_type == new_type) {
		switch (was_type) {
		case FIX_ARRAY:
		case FIX_STRING:
			// Join two pieces together
			if ((g_globConversionState == CONVERTING || g_globConversionState == CONVERTING_UNGREEDY)
				&& (g_new.m_P[offset-1] == 'r')) {
				// Join at the raw marker right behind the START_QUOTE
				P = burp_insert(&g_new, offset-1, "+");
			} else {
				// Join at the START_QUOTE
				P = burp_insert(&g_new, offset, "+");
			}
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

// substitute(): Traverses g_buffer, performing expansion & substitution
// on each subexpression we see along the way and writing the result to g_new. Compare to
// emit_without_substituting(), which only does the former and is intended for FIX_EXPRESSIONs.
// Returns the type of the result (FIX_INT, etc.)

/* Convert double quotes so that all non-escaped bracketting double quotes
 * are given clear internal codes  to simplify subsequent logic 
 * Recursively expand expressions as needed
 */

static fix_typeE substitute(fix_typeE want)
{
	fix_typeE	got;	// What I had
	fix_typeE	got1;	// What I'm now seeing
	fix_typeE	want1;
	int 		i, startquote_offset, offset, quoted, c, output_c;
	const int   NOT_QUOTED = -1;
	char		**arrayPP, *P, *P1, glob_type;
	_BOOL      	is_file_expansion, quote_removal;
	_BOOL		is_outside_quotes, burp_extra_glob_symbols = FALSE;

	log_enter("substitute (want=%t)", want);

	switch (want) {
	case FIX_INT:
	case FIX_VAR:
		if (P = integerStringOrEmpty(g_buffer.m_P)) {
			burps(&g_new, P);
			log_return_msg("%s = simple integer, nothing to substitute", P);
			return FIX_INT;
		}
	}

	is_outside_quotes = TRUE;
	is_file_expansion = FALSE;
	offset = 0;

	if (g_globConversionState == INACTIVE) {  // INACTIVE by default. Changes inside string modification operators.
		for (P = g_buffer.m_P; c = *P ; ++P) {
			switch (c) {
			case '*':
			case '?':
			case '[':
				// Any unprotected globbing character should trigger globbing
				if (is_outside_quotes && !(*(P-1)=='$' || *(P-1)=='\\'))
					is_file_expansion = TRUE;
				break;
			case '"':
				is_outside_quotes = !is_outside_quotes;
				break;
			case '\\':
				// Skip/protect escaped characters
				if (P[1]) {
					++P;
				}
			}
		}
	}

	got              = FIX_NONE;   //TODO Try initting to STRING. It looks like NONE should NEVER be used
	startquote_offset = NOT_QUOTED;
	quote_removal    = FALSE;
	want1            = (is_file_expansion ? FIX_STRING : want);

	// The strange treatment of quotes in the following code is meant to trade bash quoting for python quoting.
	// For example,   echo de"fg"hi     would translate to     print("defghi").
	// The strategy is to strip the quotes from literal text in the input, determine where python
	// needs them, and insert new ones there. To keep track of this we use the markers {START/END}_QUOTE.
	// Meanwhile we cannot just toss the quotes from the special bash variable "$*".
	// That's what 'quoted' keeps track of, as you can see in the "*" handler inside emitSimpleVariable().

	for (P = g_buffer.m_P; ; ++P) {

		c = *P;

		if (c == '"') {
			// Note the quote mark and discard it.
			quote_removal = TRUE;
			continue;
		}

		switch (c) {
		case '\0':
		case '~':
		case '$':
		case '`':
			// We have a "special" character or the null terminator.
			if (startquote_offset == NOT_QUOTED) {
				quoted = quote_removal;
			} else {
				// We are in a START_QUOTE-markedup section and have reached its end. Close it with END_QUOTE.
				burpc(&g_new, END_QUOTE);

				// Combine the type of the QUOTE-markup section with the type of whatever precedes it
				got1 = FIX_STRING;
				got = combine_types(startquote_offset, want1, got, got1);
				startquote_offset = NOT_QUOTED;
				quoted = 1;
			}
			if (!c) {  // '\0' means we are done processing
				goto done;
			}
			quote_removal = FALSE;
			offset = g_new.m_lth;
			P1 = emit_delimited(P, quoted, want1, &got1);
			if (g_globConversionState != INACTIVE) // This can change in emit_delim() so check it again
				is_file_expansion = FALSE;
			if (P1 && P1 != P) {
				got = combine_types(offset, want1, got, got1);
				P   = --P1;
				continue;    // N.B. this changes the control flow.
			}
			// Restore to where we were. //TODO is this always correct even if emit_delim() changes the buffer length?
			g_new.m_lth = offset;
			g_new.m_P[offset] = '\0';
			break;
		} // end switch block

		// Other characters are generally copied straight from input to output...
		output_c = c;

		// ... but we need to make exceptions when converting shell globbing expressions to pythonic regexes
		switch (g_globConversionState) {
        case CONVERTING:
        case CONVERTING_UNGREEDY:
			//TODO A perfect solution would use recursion to handle nested glob_types incl. NULLs

			switch (c) {
			// Look for the symbols relevant to extended globbing, POSIX globbing or both
			case '+':
				if (*(P+1) == '(') {
					glob_type = c; // Save the glob symbol, print it later
					output_c = *++P;	  // Skip to the '(', print that now
				}
				break;
			case '!':
				if (*(P+1) == '(') {
					// '!(..)' becomes '(?!..)'
					burp_extra_glob_symbols = TRUE;
					glob_type = c;
					output_c = *++P;
				}
				break;
			case '@':
				if (*(P+1) == '(') {
					output_c = *++P; // Don't flag as glob and skip the '@' sign
								// What bash sees as @(..) python sees as (..)
				}
				break;
			case ')':
				// Make sure the ')' is globbing-related
				if (glob_type != 0) {
					// Set the stage to burp both ')' and the glob type.
					burp_extra_glob_symbols = TRUE;
					output_c = *P;
				}
				break;
			case '*':
				// '*' can indicate an extended glob OR a POSIX glob
				if ((*(P+1) == '(')) {
					glob_type = c;
					output_c = *++P;
				} else {
					// Need to convert "*" to ".*"
					glob_type = c;
					output_c = '.';
					burp_extra_glob_symbols = TRUE;
				}
				break;
			case '?':
				// '?' can indicate an extended glob OR a POSIX glob
				if ((*(P+1) == '(')) {
					glob_type = c;
					output_c = *++P;
				} else {
					// We will just replace '?' with '.'. No need to remember the type.
					glob_type = 0;
					output_c = '.';
				}
				break;
				// case '[':  // Bracketed globs are the same in bash and python. Nothing to do here.
			default:
				output_c = c;
			} // switch (c)
		break;
		case PROTECTING:
			// Having ensured the glob-regex conversion is only attempted once
			// for a given input, we revert to normal execution in the future
			g_globConversionState = INACTIVE;
			break;
        } // switch (g_globConversionState)

		quote_removal = FALSE;

		// Saw some normal character: Make sure it gets QUOTED. Escape it if needed. Then post the character.
		if (startquote_offset == NOT_QUOTED) {
			// Set the START_QUOTE marker for a new subsection of text that will need quoting
			if (g_globConversionState == CONVERTING || g_globConversionState == CONVERTING_UNGREEDY) {
				// Put a raw string marker before the quote
				burpc(&g_new, 'r');
			}
			startquote_offset = g_new.m_lth;
			burpc(&g_new, START_QUOTE);
		}
		if (c == '\\') {
			if (P[1]) {
				burpc(&g_new, '\\');
				output_c = *++P;
		}	}
		// Most important step: Append the possibly-modified character to output
		burpc(&g_new, output_c);

		// If glob conversion requires us to print more symbols, do that now
		if (burp_extra_glob_symbols) {
			if (glob_type == '!')
				burpc(&g_new, '?');
			burpc(&g_new, glob_type);
			// Also append the ungreedy sign '?' if needed
			if ((glob_type == '+' || glob_type == '*') && g_globConversionState == CONVERTING_UNGREEDY)
				burpc(&g_new, '?');
			// Reset the state
			burp_extra_glob_symbols = FALSE;
			glob_type = 0;
		}
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
// because we will later be calling translate_arithmetic_expr

static void expand_without_substituting(void)
{
	int 		i, c, offset;
	char		*P, *P1;
	fix_typeE	got1;
	_BOOL		is_quoted;

	if (P = integerStringOrEmpty(g_buffer.m_P)) {
		burps(&g_new, P);
		return;
	}

	is_quoted = FALSE;
	for (P = g_buffer.m_P; c = *P; ++P) {
		offset = g_new.m_lth;
		P1 = emit_delimited(P, is_quoted, FIX_EXPRESSION, &got1);
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

static void rename_keywords(void)
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

// compactWhiteSpace(): Merge consecutive spaces in g_buffer into one
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

// unmarkQuotes(): Replace {START/END}_QUOTE markers with '"' characters
// in g_buffer. (These markers are sometimes added by substitute().)
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
				// Delete adjacent END-START pairs to merge quoted chunks
				++P;
				continue;
			}  // N.B. We fall through here
		case START_QUOTE:
			if (delete_quotes && !expand_depth) {
				continue;
			}
			// Normal step: Turn START/END_QUOTE into '"'
			c = '"';
		}
		*P1++ = c;
	}
	*P1 = '\0';
	g_buffer.m_lth = P1 - g_buffer.m_P;
}

// unmarkExpand(): Remove the {START/END}_EXPAND markers from g_buffer
// which were inserted by emit_delimited()
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
	

// fix_string1(): Second-highest transformation filter after fixBraced()
// Transforms g_buffer and returns a pointer to it
// N.B. The call chain is: fix_string()
//             -> fixBraced()     // processes "{1..10}" and "foo{a,bbb,bb}bar" constructs only
//             -> fix_string1()
//             -> [[ substitute() | expand_without() ]] // expand and [[ substitute | leave alone ]]
//                                                      // and note whether string is outer-quoted
//             -> emit_enclosed_subexpr()    // string gets stripped of its head:  ${ ` ~ $(( etc
//             -> emit_delimited()           // delimits string with START/END_EXPAND
// Then the last of these plays volleyball with the emit...() functions.
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
		// Expand and substitute
		is_expression           = FALSE;
		g_dollar_expr_nesting_level = 0;
		got = substitute(want);	
	} else {
	    // Expand only, will translate as a numeric expression in a moment
		is_expression           = TRUE;
		g_dollar_expr_nesting_level = 1;
		expand_without_substituting();
	}

	swap_burps(&g_buffer, &g_new);   // Move changes ("new") to the permanent buffer

	compactWhiteSpace();
	rename_keywords();

	unmarkQuotes(is_expression);
	unescapeDollar();
	if (is_expression) {
		char 		*translationP;

		if (translationP = translate_arithmetic_expr(g_buffer.m_P, FALSE)) {
			got         = FIX_EXPRESSION;
			g_new.m_lth = 0;
			burps(&g_new, translationP);
			swap_burps(&g_buffer, &g_new);
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
// Can return a pointer to g_buffer or g_braced
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
	    // Transform array into a pythonic list, writing [x, y, z] to g_braced
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
log_activate();
				P = fix_string1(single_want_type, gotP);
log_deactivate();
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
			return resultP;  // pointer to g_braced
		}
	} // close array block

pass_through:
	string_to_buffer(startP);
	resultP = fix_string1(want, gotP);

log_activate();
	log_return_msg("Brace expr not detected, pass through to delimitSpecialSubexpr()");
	return resultP;
}

// The top-level API for converting bash value expressions to python.
// Basically wraps fixBracedString(), which wraps ...
char * fix_string(const char *stringP, fix_typeE want, fix_typeE *gotP)
{
	char *P = NULL;

	if (want == FIX_NONE || !*stringP) {
		return (char *) stringP;
	}

	g_dollar_expr_nesting_level = 0;

	P = fixBracedString(stringP, want, gotP);

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
