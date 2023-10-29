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
	int m_exception;
	// Things to include
	struct usesS {
		int m_sys;
		int m_os;
		int m_subprocess;
		int	m_signal;
		int	m_threading;
		int m_glob;
		int m_re;
		int	m_stat;
		int m_print;
	} m_uses;
	// Basic functions needed
	struct functionS {
		int m_is_defined;
		int m_get_variable;
        int m_make;
		int m_get_value;
		int m_set_value;
		int m_str;
		int m_array;
		int m_glob;
	} m_function;
	// Expansions performed
	struct expandS {
		int m_star;
		int m_at;
		int m_hash;
		int m_dollar;
		int m_exclamation;
		int m_underbar;
		int m_hyphen;

		int	m_minus;
		int	m_plus;
		int	m_eq;
		int	m_qmark;

		int	m_colon_minus;
		int	m_colon_plus;
		int	m_colon_eq;
		int	m_colon_qmark;

		int m_prefixStar;
		int	m_prefixAt;
		int	m_indicesStar;
		int m_indicesAt;
	} m_expand;
	// Methods of Bash2Py
	struct valueS {
		int m_uses;
		int m_set_value;
		int m_preincrement;
		int m_postincrement;
		int m_is_null;
		int m_not_null_else;

		int m_plus;
		int m_minus;
		int m_multiply;
		int m_idivide;
		int m_mod;
		int m_lsh;
		int m_rsh;
		int m_band;
		int m_bor;
		int m_bxor;
	} m_value;
} translateT;

char *endQuotedString(char *startP);
char *endArray(char *startP);
char *endExpand(char *startP);

int  translate_expression(char *startP, char **translationPP, int allow_array);

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
