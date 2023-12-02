#include "config.h"

#ifdef BASH2PY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "burp.h"

typedef int _BOOL;
#define TRUE 1
#define FALSE 0
FILE *outputF;
//outputF = stdout; // TODO Change, of course.

#if !HAVE_STPCPY
char *stpcpy(char *s1, const char *s2) {
    char *end;
    do {  
        *s1++ = *s2;
    } while (*s2++);
    return --s1;
#endif
#if !HAVE_STRDUP
char *strdup(const char *s) {
    char *ret = (char *) malloc(strlen(s)+1);
    strcpy(ret, s);
    return ret;
}
#endif

unsigned char g_left_margin=0, g_return_count=0, g_if_count=0, g_asgn_count=0;
unsigned char g_lines_in_func = 0, g_conditional_nesting = 0;
_BOOL g_inside_class = FALSE, g_is_static = FALSE;
char _EXCEPT[] = "Bash2PyException";

char **g_text_expansions, **g_func_expansions;

void write_function(char *first, ...);

void init_macro_dictionaries(void)
{
    g_text_expansions = (char**) malloc (128 * sizeof(char *));
    memset(g_text_expansions, '\0', 128);
    g_text_expansions['n'] = strdup("name");
    g_text_expansions['v'] = strdup("value");
    g_text_expansions['u'] = strdup("val");
    g_text_expansions['b'] = strdup("variable");
    g_text_expansions['l'] = strdup("local");
    g_text_expansions['s'] = strdup("self");
    g_text_expansions['g'] = strdup("glob");
    g_text_expansions['r'] = strdup("ret");
    g_text_expansions['f'] = strdup("prefix");
    g_text_expansions['c'] = strdup("colon");
    g_text_expansions['q'] = strdup("== ''");
    g_text_expansions['m'] = strdup("minus");
    g_text_expansions['p'] = strdup("plus");
    g_text_expansions['t'] = strdup("star");
    g_text_expansions['N'] = strdup("None");
    g_text_expansions['a'] = strdup("sys.argv");
    g_text_expansions['S'] = strdup("$s.$v");  // self.value
    g_text_expansions['L'] = strdup("$s.$u");  // self.val
    g_text_expansions['Z'] = strdup("\"Bash $b \" + $n + \" is ~defined|$N\"");
    g_text_expansions['|'] = strdup(" or ");
    g_text_expansions['\\'] = strdup(" in ");
    g_text_expansions['0'] = strdup("is $N");
    g_text_expansions['~'] = strdup("not ");

    g_func_expansions = (char**) malloc (128 * sizeof(char *));
    memset(g_func_expansions, '\0', 128);
    g_func_expansions['S'] = strdup("Set$V");
    g_func_expansions['G'] = strdup("Get$V");
    g_func_expansions['s'] = strdup("sys.argv");
    g_func_expansions['g'] = strdup("globals");
    g_func_expansions['D'] = strdup("IsDefined");
    g_func_expansions['i'] = strdup("isinstance");
    g_func_expansions['b'] = strdup("Get$B");
    g_func_expansions['L'] = strdup("local");
    g_func_expansions['l'] = strdup("len");
    g_func_expansions['j'] = strdup("join");
}

char *_expand_macros_internal(char *str, _BOOL define_exception)
{
    static char working_str[128];
    char *p = working_str, *p_end;

    if (!str) return NULL;
    memset(working_str, '\0', 128);
    p_end = stpcpy(working_str, str);

    // Process all the expansion flags in the string
    while (p = strpbrk(p, "([$|\\~")) {
        int offal_length;
        _BOOL do_camel_case = FALSE;
        char *substitution = NULL, *where_to = NULL;
        switch (*p)
        {
            case '(':
            case '[':
                // Function & subscript macros are indicated by the ( or [ symbol
                // and the PRECEDING character if it is not part of a longer
                // identifier. So we check the two preceding characters.
                char preceding = *(p-1), next_preceding = *(p-2);
                if (p>working_str && (substitution = g_func_expansions[preceding])
                          && (&preceding==working_str /*ie nothing precedes p-1*/ || !isalnum(next_preceding))) {
                    // If indicated, allow function macros in the declaration/header
                    // but treat them differently, throwing out the parens; e.g. "f()" expands to "foo"
                    if (define_exception && *(p+1) == ')') {
                        offal_length = 3;
                    } else {
                        offal_length = 1;
                    }
                    where_to = p-1;
                }
                break;
            case '$':
                // General text macros: indicated by '$' followed by one letter/number
                char next = *(p+1);
                if (isalnum(next)) {
                    if (substitution = g_text_expansions[next]) {
                        offal_length = 2;
                        where_to = p;
                    }
                    // Forced upper-casing: Given a capital-letter micro having no
                    // dictionary entry, we look for a lowercase micro. If found 
                    // we make the substitution and convert it to title case.
                    else if (isupper(next)) {
                        if (substitution = g_text_expansions[tolower(next)]) {
                            do_camel_case = TRUE;
                            offal_length = 2;
                            where_to = p;
                        }
                    }
                }
                break;
            case '|':
            case '\\':
            case '~':
                if ((*p=='|') && strchr("|=", *(p+1))) {
                    // Leave literal '|' characters alone when we need to
                    p++; break;
                }
                substitution = g_text_expansions[*p];
                offal_length = 1;
                where_to = p;
                break;
        }
        if (where_to) {
            char *tail = where_to + offal_length;
            int substitution_len = strlen(substitution);
            memmove(where_to+substitution_len, tail, (int)(p_end-tail));
            memcpy(where_to, substitution, substitution_len);
            if (do_camel_case) *where_to = toupper(*where_to);
            p_end += substitution_len - offal_length;
        }
        // Advance unless substitution reveals another substitution to be made
        if (*p != '$' || !substitution)
            p++;
    }

    return working_str;
}

char *expand_macros(char *name) { return _expand_macros_internal(name, FALSE); }

void dispose_macro_dictionaries(void)
{
    for (int i=0; i<sizeof(g_text_expansions); i++)
        if (g_text_expansions[i])
            free(g_text_expansions[i]);
    free(g_text_expansions);
    for (int i=0; i<sizeof(g_func_expansions); i++)
        if (g_func_expansions[i])
            free(g_func_expansions[i]);
    free(g_func_expansions);
}

char *_static() {
    static char sm[32];
    memset(sm, ' ', g_left_margin); 
    strcpy(sm+g_left_margin, "@staticmethod\n");
    return sm;
}

void _set_static(_BOOL is_static) { g_is_static = is_static; };

char *_add_to_list(char *s1, char *s2) {
    /* dangerous memory modification */
    char *p = memchr(s1, '\0', 80);
    stpcpy(stpcpy(p, s2), ", ");
    return s1;
}

char *_def(char *name, char *sig)
{
    static char py_buf[80];
    char value_buf[16];
    char *p = py_buf;
    int len;

    // Do NOT init/reset the subsystem state because not all objects start with 
    // def() even though most of them do. We will reset in write_func() instead.
    g_left_margin = g_inside_class ? 2 : 0;

    // Indent and begin
    if (g_is_static) p = stpcpy(py_buf, _static());
    memset(p, '\0', sizeof(py_buf)-(p-py_buf));
    memset(p, ' ', g_left_margin);
    sprintf(p+g_left_margin, "def %s(", _expand_macros_internal(name, TRUE));

    // Build list of args
    if (sig) {
        // Check for every possible flag in "bash2py standard" order
        if (p = strchr(sig,'S')) _add_to_list(py_buf, "self");
        if (p = strchr(sig,'N')) _add_to_list(py_buf, "name");
        if (p = strchr(sig,'V')) {
            sprintf(value_buf, "value%s", (p[1] == '0') ? "=None" : 
                                     (p[1] == '\'' ? "=''" : 
                                      ""));
            _add_to_list(py_buf, value_buf);
        }
        if (p = strchr(sig,'L')) _add_to_list(py_buf, "local=locals()");
        if (p = strchr(sig,'I')) _add_to_list(py_buf, "inc=1");
        if (p = strchr(sig,'Q')) _add_to_list(py_buf, "in_quotes");
    }

    // Terminate the list of args and close the header
    len = strlen(py_buf);
    if (py_buf[len-1] == ' ') py_buf[len-2] = '\0';
    strcat(py_buf, "):\n");
    g_left_margin += 2;
    g_lines_in_func++;
    return py_buf;
}

char *_def0(char *name) { return _def(name, NULL); }

char *_ret_internal(char *return_what) {
    static char rets[3][48];
    char *ret = rets[g_return_count++];
    assert(g_return_count <= 3);

    memset(ret, ' ', g_left_margin);
    if (return_what)
        sprintf(ret+g_left_margin, "return %s\n", return_what);
    else
        strcpy(ret+g_left_margin, "return ret\n");
    g_left_margin -= 2;
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return ret;
}
char *_ret(char *return_what) { return _ret_internal(expand_macros(return_what)); }

char *_raise_internal(char *desc, _BOOL need_quotes) {
    static char _raised[64];
    char quote[2];
    strcpy(quote, need_quotes ? "\"" : "");
    memset(_raised, ' ', g_left_margin);
    sprintf(_raised+g_left_margin, "raise %s(%s%s%s)\n", _EXCEPT, quote, desc, quote);
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return _raised;
}
char *_raise(char *desc) { return _raise_internal(expand_macros(desc), FALSE); }

// "if" and "else" clauses. For the latter, set the argument to NULL.
char *_if(char *_if, char *types, ...);
char *_else(char *types, ...);

char *_if_internal(char *cond) {
    static char if_stmts[3][256];
    char *if_stmt = if_stmts[g_if_count++];
    assert(g_if_count <= 3);

    memset(if_stmt, ' ', g_left_margin);
    if (cond)
        sprintf(if_stmt+g_left_margin, "if %s:\n", cond);
    else
        sprintf(if_stmt+g_left_margin, "else:\n");
    g_left_margin += 2;
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return if_stmt;
}
//char *_if(char *cond) { return _if_internal(expand_macros(cond)); }

char *_asgn_internal(char *l_name, char *oper, char *r_name) {
    static char ops[3][128]; 
    char *op = ops[g_asgn_count++];
    assert(g_asgn_count <= 3);

    if (g_inside_class && g_left_margin == 0)
        g_left_margin = 2;
    memset(op, '\0', 128);
    memset(op, ' ', g_left_margin);
    sprintf(op+g_left_margin, "%s %s= %s\n", l_name, oper?oper:"", r_name);
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return op;
}
char *_asgn(char *l_name, char *r_name) { 
    char *expanded_lname = strdup(expand_macros(l_name));
    char *result = _asgn_internal(expanded_lname, NULL, expand_macros(r_name));
    free(expanded_lname);
    return result;
}

void _assignment_func_internal(char *name, char *type_signature, char *oper)
{
    char *def, *tmp_asgn, *asgn, *p;
    _BOOL is_initializer = (0 == strcmp(name , "__init__"));
    _BOOL has_tmp_line = (p = strchr(type_signature, '+')) != NULL;
    if (p) *p = '\0';

    g_left_margin = (g_inside_class ? 2 : 0);
    def = _def(name, type_signature);
    if (has_tmp_line)
        tmp_asgn = _asgn("tmp", is_initializer ? "$S" : "$L");
    asgn = _asgn_internal("self.val", oper, (strchr(type_signature, 'I') ? "inc" : "value"));
    if (is_initializer)
        write_function(def, asgn);
    else if (has_tmp_line)
        write_function(def, tmp_asgn, asgn, _ret("tmp"));
    else
        write_function(def, asgn, _ret("$L"));
}

void write_assignment_func(char *name, char *oper) { 
    _assignment_func_internal(name, "SV", oper);
}
void write_init_func(char *sig) { 
    _assignment_func_internal("__init__", sig, NULL);
}

void write_increment_func(char *name, char *oper) {
    char op[3], signature[4];
    if (0 == strcmp(oper, "++")) {
        strcpy(op, oper); op[1]='\0';
        strcpy(signature, "SI+");
    } else {
        strcpy(op, oper);
        strcpy(signature, "SI");
    }
    _assignment_func_internal(name, signature, op);
}

void write_unsupported_func(char *name, _BOOL has_name_arg, char *bash_expr) {
    char exc_desc[32];
    char *def, *raise;

    g_left_margin = 2;
    def = _def(name, has_name_arg?"N":NULL);
    sprintf(exc_desc, "%s unsupported", bash_expr);
    raise = _raise_internal(expand_macros(exc_desc), TRUE);
    write_function(def, raise);
    g_left_margin = 0;
}
void write_unsupported_func0(char *name, char *bash_expr) {
    write_unsupported_func(name, FALSE, bash_expr);
}

char *_process_ifelse_subordinates(char *buf, char *types, va_list *thens)
{
    char *p_buf = memchr(buf, '\0', 256);

    // Process the subordinate stmts one-by-one
    char *p_type = types;
    char *stmt, *expanded_stmt, *next, *next_types;
    g_conditional_nesting++;
    while (*p_type)
    {
        stmt = va_arg(*thens, char *);

        switch (*p_type) {
            case 'A':
                expanded_stmt = strdup(expand_macros(stmt));
                next = va_arg(*thens, char *);
                p_buf = stpcpy(p_buf, _asgn_internal(expanded_stmt, NULL, expand_macros(next)));
                free(expanded_stmt);
                break;
            case 'I': 
                // As in _if(), process the "if" line then the subordniate lines
                p_buf = stpcpy(p_buf, _if_internal(expand_macros(stmt)));
                next_types = va_arg(*thens, char *);
                p_buf = _process_ifelse_subordinates(p_buf, next_types, thens);
                break;
            case 'R':
                p_buf = stpcpy(p_buf, _ret_internal(expand_macros(stmt)));
                break;
            case 'r':
                p_buf = stpcpy(p_buf, _raise_internal(expand_macros(stmt), FALSE));
                break;
            case 'L': // literal text
                memset(p_buf, ' ', g_left_margin);
                p_buf = stpcpy(p_buf+g_left_margin, expand_macros(stmt));
                p_buf = stpcpy(p_buf,"\n");
                break;
            default:
                printf("Invalid 'then' stmt type '%c'\n", *p_type);
                assert(0);
            }
        p_type++;
    }
    g_conditional_nesting--;
    return p_buf;
}


char *_else(char *types, ...)
{
    // Process the else clause before its subordinate stmts
    int begin_margin = g_left_margin;
    char *buf = _if_internal(NULL);

    va_list thens;
    va_start(thens, types);
    _process_ifelse_subordinates(buf, types, &thens);

    g_left_margin = begin_margin;
    va_end(thens);
    return buf;
}

char *_if(char *_if, char *types, ...)
{
    // Process the if clause before its subordinate stmts
    int begin_margin = g_left_margin;
    char *buf = _if_internal(expand_macros(_if));

    va_list thens;
    va_start(thens, types);
    _process_ifelse_subordinates(buf, types, &thens);

    g_left_margin = begin_margin;
    va_end(thens);
    return buf;
}

void _cls(char *name, _BOOL is_exception)
{
    char type[10];
    g_left_margin = 0;

    strcpy(type, is_exception ? "Exception" : "Object");
    fprintf(outputF, "class %s(%s):\n", name, type);
    g_inside_class = TRUE;
    g_is_static = TRUE;
    return;
}

void _end_cls(void)
{
    g_left_margin = 0;
    g_inside_class = FALSE;
    g_is_static = FALSE;
    fprintf(outputF, "\n");
}


// Stream a function line-by-line
void write_function(char *first, ...) // First arg is broken out to comply with ANSI C
{
    int i;
    va_list lines;
    char *fmt = (char*) malloc(g_lines_in_func*2 + strlen(first)+1);
    char *pFmt;

    strcpy(fmt, first);
    for(i=1, pFmt=memchr(fmt, '\0', 64); i<g_lines_in_func; i++)
    {
        *pFmt++ = '%';
        *pFmt++ = 's';
    }
    if (!g_inside_class)
        *pFmt++ = '\n';
    *pFmt = '\0';

    if (g_inside_class)
        fprintf(outputF, "        #\n");
    va_start(lines, fmt);
    vfprintf(outputF, fmt, lines);
    va_end(lines);
    g_lines_in_func = 0;
    free(fmt);

    // Reset the system state for the next python function or object
    g_return_count = 0;
    g_if_count = 0;
    g_asgn_count = 0;
}

#ifdef TEST

char *run_test()
{
    char *def, *asgn, *iff, *iff2, *els, *ret;

    // Create the lines of each python function in the order they will be printed,
    // then print them all at once. This ensures that the static buffers will 
    // not become garbled when we reuse them and avoids problems that can arise
    // from the unpredictable evaluation order of function arguments 

    init_macro_dictionaries();
    outputF = stdout;//TODO change this, of course

    // Exception class
    _cls(_EXCEPT, TRUE);
        _set_static(FALSE);
        write_init_func("SV0");

        def = _def("__str__", "S");
//TODO choose either-or:       ret = _ret("repr($S)");
        write_function(def, _ret("repr($S)"));//TODO ret);
    _end_cls();

    // Global functions
    def = _def("D()", "NL");
    ret = _ret("$n\\g()|$n\\$l");
    write_function(def, ret);

    def = _def("b()", "NL");
    iff = _if("$n\\$l", "R", "L[$n]");
    iff2 = _if("$n\\g()", "R", "g()[$n]");
    ret = _ret("$N");
    write_function(def, iff, iff2, ret);

    def = _def("Make", "NL");
    asgn = _asgn("$r", "b($n, $l)");
    iff = _if("$r $0", "AA", "$r", "Bash2Py(0)", "g()[$n]", "$r");
    ret = _ret(NULL);
    write_function(def, asgn, iff, ret);

    def = _def("G()", "NL");
    asgn = _asgn("$b", "b($n,$l)");
    iff = _if("$b $0|$b.$u $0", "R", "''");
    ret = _ret("$b.$u");
    write_function(def, asgn, iff, ret);

    def = _def("S()", "NVL");
    asgn = _asgn("$b", "b($n,$l)");
    iff = _if("$b $0", "A", "g()[$n]","Bash2Py($v)");
    els = _else("A", "$b.$u", "$v");
    ret = _ret("$v");
    write_function(def, asgn, iff, els, ret);

    def = _def("Str", "V");
    iff = _if("i($v, list)", "R", "\" \".j($v)");
    iff2 = _if("i($v, basestring)", "R", "$v");
    ret = _ret("str($v)");
    write_function(def, iff, iff2, ret);

    def = _def("Array", "V");
    iff = _if("i($v, list)", "R", "$v");
    iff2 = _if("i($v, basestring)", "R", "$v.strip().split(' ')");
    ret = _ret("[ $v ]");
    write_function(def, iff, iff2, ret);

    def = _def("$G", "V");
    asgn = _asgn("$r", "$g.$g($v)");
    iff = _if("l($r) < 1", "A", "$r", "[ $v ]");
    ret = _ret(NULL);
    write_function(def, asgn, iff, ret);

    // Expand class
    _cls("Expand", FALSE);
        def = _def0("at");
        iff = _if("l($a) < 2", "R", "[]");
        ret = _ret("s[1:]");
        write_function(def, iff, ret);

        def = _def("$t", "Q");
        iff = _if("in_quotes", "IR", /*I:*/ "l($a) < 2", "R", "\"\"",
                                     /*R:*/ "\" \".j(s[1:])");
        ret = _ret("Expand.at()");
        write_function(def, iff, ret);

        def = _def0("hash");
        ret = _ret("l($a)-1");
        write_function(def, ret);

        def = _def0("dollar");
        ret = _ret("os.getpid()");
        write_function(def, ret);

        write_unsupported_func0("exclamation", "$!");
        write_unsupported_func0("underbar", "$_");
        write_unsupported_func0("hyphen", "$-");

        def = _def("$m", "NV\'");
        iff = _if("D($n)", "R", "G($n)");
        ret = _ret("$v");
        write_function(def, iff, ret);

        def = _def("eq", "NV\'");
        iff = _if("~D($n)", "LR", "S($n, $v)", "$v");
        ret = _ret("G($n)");
        write_function(def, iff, ret);

        def = _def("qmark", "NV0");
        iff = _if("~D($n)", "Ir", /*I:*/ "$v $0|$v == \'\'", "A", "$v", "$Z", 
                                   /*r:*/ "$v");
        ret = _ret("G($n)");
        write_function(def, iff, ret);

        def = _def("$p", "NV\'");
        iff = _if("~D($n)", "R", "''");
        ret = _ret("$v");
        write_function(def, iff, ret);

        def = _def("$c$M", "NV\'");
        asgn = _asgn("$r", "G($n)");
        iff = _if("$r $0|$r $q", "A", "$r", "$v");
        ret = _ret("$r");
        write_function(def, asgn, iff, ret);

        def = _def("$cEq", "NV\'");
        asgn = _asgn("$r", "G($n)");
        iff = _if("$r $0|$r $q", "LA", /*L:*/ "S($n, $v)", /*A:*/ "$r", "$v");
        ret = _ret("$r");
        write_function(def, asgn, iff, ret);

        def = _def("$cQmark", "NV0");
        asgn = _asgn("$r", "G($n)");
        iff = _if("$r $0|$r $q", "I", "$v $0|$v $q", "Ar",
                       /*A:*/ "$v", "$Z", /*r:*/ "$v");
        ret = _ret("$r");
        write_function(def, asgn, iff, ret);

        def = _def("$c$P", "NV\'");
        asgn = _asgn("$r", "G($n)");
        iff = _if("$r $0|$r $q", "R", "''");
        ret = _ret("$v");
        write_function(def, asgn, iff, ret);

        write_unsupported_func("$f$T", TRUE, "${!$f*}");
        write_unsupported_func("$fAt", TRUE, "${!$f@}");
        write_unsupported_func("indices$T", TRUE, "${!$n[*]}");
        write_unsupported_func("indicesAt", TRUE, "${!$n[*]}");

    _end_cls();

    _cls("Bash2Py", FALSE);
        _set_static(FALSE);

        asgn = _asgn("__slots__", "[\"$u\"]");
        write_function(asgn);  // kludge
        write_init_func("SV\'");

        def = _def("S()", "SV0");
        asgn = _asgn("$L", "$v");
        ret = _ret("$v");
        write_function(def, asgn, ret);

        write_increment_func("preinc", "+");
        write_increment_func("postinc", "++");

        def = _def("isNull", "S");
        ret = _ret("$L $0");
        write_function(def, ret);

        def = _def("notNullElse", "SV");
        iff = _if("$s.isNull()", "R", "$v");
        ret = _ret("$L");
        write_function(def, iff, ret);

        write_assignment_func("$p", "+");
        write_assignment_func("$m", "-");
        write_assignment_func("multiply", "*");
        write_assignment_func("idivide", "//");
        write_assignment_func("mod", "%%");
        write_assignment_func("lsh", "<<");
        write_assignment_func("rsh", ">>");
        write_assignment_func("band", "&");
        write_assignment_func("bor", "|");
        write_assignment_func("bxor", "^");

    _end_cls();

    dispose_macro_dictionaries();
}

/*************************************************************/

int main()
{
    run_test();
    return 0;
}

#endif // #ifdef TEST
#endif // #ifdef BASH2PY

