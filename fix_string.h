#ifndef __FIX_STRING_H__
#define __FIX_STRING_H__

typedef int _BOOL;

/* We handle 5 types of data:
	NONE, INT, STRING, VAR, ARRAY
 */
typedef enum {
	FIX_NONE=0, 	// Emit string simply returns its input unchanged
	FIX_INT, 		// Don't convert integers to strings
	FIX_STRING, 	// Full conversion of everything to strings
	FIX_VAR,		// Return whatever is seen unchanged
	FIX_ARRAY,		// Generate arrays per word
	FIX_EXPRESSION	// Return the string seen but without outer quotes
} fix_typeE;

typedef struct {
	_BOOL m_exception;
	// Things to include
	struct usesS {
		_BOOL m_sys;
		_BOOL m_os;
		_BOOL m_subprocess;
		_BOOL m_signal;
		_BOOL m_threading;
		_BOOL m_glob;
		_BOOL m_re;
		_BOOL m_stat;
		_BOOL m_print;
	} m_uses;
	// Basic functions needed
	struct functionS {
		_BOOL m_is_defined;
		_BOOL m_get_variable;
        _BOOL m_make;
		_BOOL m_get_value;
		_BOOL m_set_value;
		_BOOL m_str;
		_BOOL m_array;
		_BOOL m_glob;
	} m_function;
	// Expansions performed
	struct expandS {
		_BOOL m_star;
		_BOOL m_at;
		_BOOL m_hash;
		_BOOL m_dollar;
		_BOOL m_exclamation;
		_BOOL m_underbar;
		_BOOL m_hyphen;

		_BOOL m_minus;
		_BOOL m_plus;
		_BOOL m_eq;
		_BOOL m_qmark;

		_BOOL m_colon_minus;
		_BOOL m_colon_plus;
		_BOOL m_colon_eq;
		_BOOL m_colon_qmark;

		_BOOL m_prefixStar;
		_BOOL m_prefixAt;
		_BOOL m_indicesStar;
		_BOOL m_indicesAt;
	} m_expand;
	// Methods of Bash2Py
	struct valueS {
		_BOOL m_uses;
		_BOOL m_set_value;
		_BOOL m_preincrement;
		_BOOL m_postincrement;
		_BOOL m_is_null;
		_BOOL m_not_null_else;

		_BOOL m_plus;
		_BOOL m_minus;
		_BOOL m_multiply;
		_BOOL m_idivide;
		_BOOL m_mod;
		_BOOL m_lsh;
		_BOOL m_rsh;
		_BOOL m_band;
		_BOOL m_bor;
		_BOOL m_bxor;
	} m_value;
} translateT;

char *endQuotedString(char *startP);
char *endArray(char *startP);
char *endExpand(char *startP);

char *translate_arithmetic_expr(char *startP, _BOOL allow_array);

char *fix_string(const char *stringP, fix_typeE type, fix_typeE *gotP);

/* These characters are embedded in the translated text:
   We presume they do not occur in the input
   They are temporary and are removed/translated back before finishing

   START_QUOTE   indicates a start " that acts as a QUOTE
   END_QUOTE     indicates an end  " that acts as a QUOTE
   START_EXPAND  indicates start of something that expands
   END_EXPAND    indicates end   of something that expands
*/

#define START_QUOTE  1
#define END_QUOTE    2
#define PLACEHOLDER  3
#define START_EXPAND 4
#define END_EXPAND   5	// Must be last

#endif // __FIX_STRING_H__
