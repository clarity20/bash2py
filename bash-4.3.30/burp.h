#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

typedef struct {
	char	*m_P;
	int 	m_lth;
	int		m_max;
    int		m_indent;
	int		m_disable_indent;
	int		m_ungetc;
} burpT;

void burp_reset(burpT *burpP);
char *burp_extend(burpT *burpP, int offset, int need);
void burp(burpT *burpP, const char *fmtP, ...);
void burpc(burpT *burpP, const char c);
void burp_ungetc(burpT *burpP);
void burpn(burpT *burpP, const char *stringP, int lth);
void burps(burpT *burpP, const char *stringP);
void burps_html(burpT *burpP, const char *stringP);
void burp_esc_quote(burpT *burpP, int offset, int quote);
void burp_rtrim(burpT *burpP);

#define INDENT(X) X.m_indent++
#define OUTDENT(X) { assert(0 < X.m_indent); X.m_indent--; }

void log_init();
void log_close();
void log_activate();
void log_deactivate();
void log_enter(char *format, ...);
void log_info(char *format, ...);
void log_return();
void log_return_msg(char *msg_template, ...);
