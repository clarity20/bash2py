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

#include "translate.h"
#include "dynamo.h"

#ifdef TEST
FILE *outputF;
#endif

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

unsigned char g_left_margin, g_return_count, g_if_count, g_asgn_count;
unsigned char g_lines_in_func, g_conditional_nesting;
_BOOL g_inside_class, g_is_static;
char _EXCEPT[] = "Bash2PyException";

char **g_text_expansions = NULL, **g_func_expansions = NULL;

void write_function(char *first, ...);

void _write_line(char *dest, const char *fmt, ...) {
    char *format = malloc(32 * sizeof(char));
    memset(format, ' ', g_left_margin);
    strcpy(format+g_left_margin, fmt);

    va_list args;
    va_start(args, fmt);
    vsprintf(dest, format, args);
    va_end(args);
    free(format);
}

void init_dynamo(void)
{
    g_left_margin = 0, g_return_count = 0, g_if_count = 0, g_asgn_count = 0;
    g_lines_in_func = 0, g_conditional_nesting = 0;
    g_inside_class = FALSE, g_is_static = FALSE;

    if (g_text_expansions && g_func_expansions)
        return;

    g_text_expansions = (char**) malloc (128 * sizeof(char *));
    memset(g_text_expansions, '\0', 128 * sizeof(char *));
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
    memset(g_func_expansions, '\0', 128 * sizeof(char *));
    g_func_expansions['S'] = strdup("Set$V");
    g_func_expansions['t'] = strdup("set$V");  // lowercase "s"et
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

// Perform possibly-recursive macro substitutions on a suitably primed
// input string to produce a line of Python code
char *_expand_macros_internal(char *str, _BOOL allow_header_macros)
{
    static char working_str[128];
    char *p = working_str, *p_end;

    if (!str) return NULL;
    memset(working_str, '\0', 128);
    p_end = stpcpy(working_str, str);

    // Process all the expansion flags in the string
    while (p = strpbrk(p, "([$|\\~")) {
        int offal_length;
        _BOOL do_title_case = FALSE;
        char *substitution = NULL, *where_to_sub = NULL;
        char preceding, next_preceding, next;
        switch (*p)
        {
            case '(':
            case '[':
                // Function & subscript macros are indicated by the ( or [ symbol
                // and the PRECEDING character if it is not part of a longer
                // identifier. So we check the two preceding characters.
                preceding = *(p-1); next_preceding = *(p-2);
                if (p>working_str && (substitution = g_func_expansions[preceding])
                          && (&preceding==working_str /*ie nothing precedes p-1*/ || !isalnum(next_preceding))) {
                    // If indicated, detect function macros in the declaration/header; format = "f()"
                    // but treat them differently, throwing out the parens; e.g. "f()" expands to "foo"
                    if (allow_header_macros && *(p+1) == ')') {
                        // It's a special expansion as described that will consume 3 characters
                        offal_length = 3;
                    } else {
                        // It's a normal expansion that will consume 1 character
                        offal_length = 1;
                    }
                    where_to_sub = p-1;
                }
                break;
            case '$':
                // General text macros: indicated by '$' followed by one letter/number
                next = *(p+1);
                if (isalnum(next)) {
                    if (substitution = g_text_expansions[next]) {
                        offal_length = 2;   // We will consume '$' and "next"
                        where_to_sub = p;
                    }
                    // Forced upper-casing: Given a capital-letter micro having no
                    // dictionary entry, we look for a lowercase micro. If found 
                    // we make the substitution and convert it to title case.
                    else if (isupper(next)) {
                        if (substitution = g_text_expansions[tolower(next)]) {
                            do_title_case = TRUE;
                            offal_length = 2;
                            where_to_sub = p;
                        }
                    }
                }
                break;
            case '|':
            case '\\':
            case '~':
                // Special symbols in the encoded text having no "$" flag
                if ((*p=='|') && strchr("|=", *(p+1))) {
                    // Leave '|' characters alone when they are
                    // semantically meaningful in the working text
                    p++; break;
                }
                substitution = g_text_expansions[*p];
                offal_length = 1;
                where_to_sub = p;
                break;
        }
        if (where_to_sub) {
            char *tail = where_to_sub + offal_length;
            int substitution_len = strlen(substitution);
            memmove(where_to_sub+substitution_len, tail, (int)(p_end-tail));
            memcpy(where_to_sub, substitution, substitution_len);
            if (do_title_case) *where_to_sub = toupper(*where_to_sub);
            p_end += substitution_len - offal_length;
        }
        // Advance in the updated working text unless substitution reveals
        // another substitution to be made at the current position
        if (*p != '$' || !substitution)
            p++;
    }

    return working_str;
}

char *expand_macros(char *name) { return _expand_macros_internal(name, FALSE); }

void cleanup_dynamo(void)
{
    for (int i=0; i<sizeof(g_text_expansions); i++)
        if (g_text_expansions[i])
            { free(g_text_expansions[i]); g_text_expansions[i] = NULL; }
    free(g_text_expansions); g_text_expansions = NULL;
    for (int i=0; i<sizeof(g_func_expansions); i++)
        if (g_func_expansions[i])
            { free(g_func_expansions[i]); g_func_expansions[i] = NULL; }
    free(g_func_expansions); g_func_expansions = NULL;

    g_left_margin = 0, g_return_count = 0, g_if_count = 0, g_asgn_count = 0;
    g_lines_in_func = 0, g_conditional_nesting = 0;
    g_inside_class = FALSE, g_is_static = FALSE;
}

char *_static() {
    static char sm[32];
    _write_line(sm, "@staticmethod\n");
    return sm;
}

void set_static(_BOOL is_static) { g_is_static = is_static; };
char *_add_to_parm_list(char *l, const char *s) { return stpcpy(stpcpy(l, s), ", "); }

char *def_(char *name, char *sig)
{
    static char py_buf[80];
    char value_buf[16];
    char *p = py_buf;
    int len;

    // Do NOT init/reset the dynamo state because the objects do not need to start with
    // def() even though most of them do. We will reset in write_func() instead.
    g_left_margin = g_inside_class ? 2 : 0;

    // Indent and begin
    if (g_is_static) p = stpcpy(p, _static());
    _write_line(p, "def %s(", _expand_macros_internal(name, TRUE));
    p = memchr(p, '\0', sizeof(py_buf));

    // Build the argument list
    if (g_inside_class && !g_is_static) {
        p = _add_to_parm_list(p, "self");
    }
    if (sig) {
        // Check for every possible flag in "bash2py standard" order
        char *s = sig;
        if (strchr(sig,'N')) p = _add_to_parm_list(p, "name");
        if (s = strchr(sig,'V')) {
            sprintf(value_buf, "value%s", (s[1] == '0') ? "=None" :
                                          (s[1] == '\'' ? "=''" :
                                           ""));
            p = _add_to_parm_list(p, value_buf);
        }
        if (strchr(sig,'L')) p = _add_to_parm_list(p, "local=locals()");
        if (strchr(sig,'I')) p = _add_to_parm_list(p, "inc=1");
        if (strchr(sig,'Q')) p = _add_to_parm_list(p, "in_quotes");
    }

    // Terminate the list of args and close the header
    if (*(p-1) == ' ') { --p; *--p = '\0'; }
    strcpy(p, "):\n");
    g_left_margin += 2;
    g_lines_in_func++;
    return py_buf;
}

char *def0_(char *name) { return def_(name, NULL); }

char *_ret_internal(char *return_what) {
    static char rets[3][48];
    char *ret = rets[g_return_count++];
    assert(g_return_count <= 3);

    _write_line(ret, "return %s\n", return_what ? return_what : "ret");
    g_left_margin -= 2;
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return ret;
}
char *ret_(char *return_what) { return _ret_internal(expand_macros(return_what)); }

char *_raise_internal(char *desc, _BOOL need_quotes) {
    static char _raised[64];
    char quote[2];
    strcpy(quote, need_quotes ? "\"" : "");
    _write_line(_raised, "raise %s(%s%s%s)\n", _EXCEPT, quote, desc, quote);
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return _raised;
}
char *raise_(char *desc) { return _raise_internal(expand_macros(desc), FALSE); }

// "if" and "else" clauses. For the latter, set the argument to NULL.
char *if_(char *if_, char *types, ...);
char *else_(char *types, ...);

char *_if_internal(char *cond) {
    static char if_stmts[3][256];
    char *if_stmt = if_stmts[g_if_count++];
    assert(g_if_count <= 3);

    _write_line(if_stmt, "%s%s:\n", cond?"if ":"else", cond?cond:"");
    g_left_margin += 2;
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return if_stmt;
}

char *_asgn_internal(char *l_name, char *operator, char *r_name) {
    static char assignments[3][128];
    char *assignment = assignments[g_asgn_count++];
    assert(g_asgn_count <= 3);

    if (g_inside_class && g_left_margin == 0)
        g_left_margin = 2;
    _write_line(assignment, "%s %s= %s\n", l_name, operator?operator:"", r_name);
    if (g_conditional_nesting == 0) g_lines_in_func++;
    return assignment;
}
char *asgn_(char *l_name, char *r_name) {
    char *expanded_lname = strdup(expand_macros(l_name));
    char *result = _asgn_internal(expanded_lname, NULL, expand_macros(r_name));
    free(expanded_lname);
    return result;
}

void _assignment_func_internal(char *name, char *type_signature, char *oper)
{
    char *def, *tmp_asgn, *asgn, *p;
    _BOOL is_initializer = (0 == strcmp(name, "__init__"));
    _BOOL has_tmp_line = (p = strchr(type_signature, '+')) != NULL;
    if (p) *p = '\0';

    g_left_margin = (g_inside_class ? 2 : 0);
    def = def_(name, type_signature);
    if (has_tmp_line)
        tmp_asgn = asgn_("tmp", is_initializer ? "$S" : "$L");
    asgn = _asgn_internal("self.val", oper, (strchr(type_signature, 'I') ? "inc" : "value"));
    if (is_initializer)
        write_function(def, asgn);
    else if (has_tmp_line)
        write_function(def, tmp_asgn, asgn, ret_("tmp"));
    else
        write_function(def, asgn, ret_("$L"));
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
    def = def_(name, has_name_arg?"N":NULL);
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
        int begin_margin;
        stmt = va_arg(*thens, char *);

        switch (*p_type) {
            case 'A':
                expanded_stmt = strdup(expand_macros(stmt));
                next = va_arg(*thens, char *);
                p_buf = stpcpy(p_buf, _asgn_internal(expanded_stmt, NULL, expand_macros(next)));
                free(expanded_stmt);
                break;
            case 'I': 
                // As in if_(), process the "if" line then the subordinate lines,
                // but do not call if_() because that would reset "thens"
                begin_margin = g_left_margin;
                p_buf = stpcpy(p_buf, _if_internal(expand_macros(stmt)));
                next_types = va_arg(*thens, char *);
                p_buf = _process_ifelse_subordinates(p_buf, next_types, thens);
                g_left_margin = begin_margin;
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


char *else_(char *types, ...)
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

char *if_(char *if_, char *types, ...)
{
    // Process the if clause before its subordinate stmts
    int begin_margin = g_left_margin;
    char *buf = _if_internal(expand_macros(if_));

    va_list thens;
    va_start(thens, types);
    _process_ifelse_subordinates(buf, types, &thens);

    g_left_margin = begin_margin;
    va_end(thens);
    return buf;
}

void cls(char *name, _BOOL is_exception)
{
    char type[10];
    g_left_margin = 0;

    strcpy(type, is_exception ? "Exception" : "object");
    fprintf(outputF, "class %s(%s):\n", name, type);
    g_inside_class = TRUE;
    g_is_static = TRUE;
    return;
}

void end_cls(void)
{
    g_left_margin = 0;
    g_inside_class = FALSE;
    g_is_static = FALSE;
    fprintf(outputF, "\n\n");
}


// Stream a function line-by-line
void write_function(char *first_line, ...) // first arg is broken out to comply with ANSI C
{
    int i;
    va_list lines;
    char *fmt = (char*) malloc(g_lines_in_func*2 + strlen(first_line)+1);
    char *pFmt = stpcpy(fmt, first_line);

    for(i=1; i<g_lines_in_func; i++)
    {
        *pFmt++ = '%';
        *pFmt++ = 's';
    }
    if (!g_inside_class)
        *pFmt++ = '\n';
    *pFmt = '\0';

    if (g_inside_class)
        fprintf(outputF, "\n");
    va_start(lines, first_line);
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

    outputF = stdout;
    init_dynamo();

    // Exception class
    cls(_EXCEPT, TRUE);
        set_static(FALSE);
        write_init_func("SV0");

        def = def_("__str__", "S");
        write_function(def, ret_("repr($S)"));
    end_cls();

    // Global functions
    def = def_("D()", "NL");
    write_function(def, ret_("$n\\g()|$n\\$l"));

    def = def_("b()", "NL");
    iff = if_("$n\\$l", "R", "L[$n]");
    iff2 = if_("$n\\g()", "R", "g()[$n]");
    write_function(def, iff, iff2, ret_("$N"));

    def = def_("Make", "NL");
    asgn = asgn_("$r", "b($n, $l)");
    iff = if_("$r $0", "AA", "$r", "Bash2Py(0)", "g()[$n]", "$r");
    write_function(def, asgn, iff, ret_(NULL));

    def = def_("G()", "NL");
    asgn = asgn_("$b", "b($n,$l)");
    iff = if_("$b $0|$b.$u $0", "R", "''");
    write_function(def, asgn, iff, ret_("$b.$u"));

    def = def_("S()", "NVL");
    asgn = asgn_("$b", "b($n,$l)");
    iff = if_("$b $0", "A", "g()[$n]","Bash2Py($v)");
    els = else_("A", "$b.$u", "$v");
    write_function(def, asgn, iff, els, ret_("$v"));

    def = def_("Str", "V");
    iff = if_("i($v, list)", "R", "\" \".j($v)");
    iff2 = if_("i($v, basestring)", "R", "$v");
    write_function(def, iff, iff2, ret_("str($v)"));

    def = def_("Array", "V");
    iff = if_("i($v, list)", "R", "$v");
    iff2 = if_("i($v, basestring)", "R", "$v.strip().split(' ')");
    write_function(def, iff, iff2, ret_("[ $v ]"));

    def = def_("$G", "V");
    asgn = asgn_("$r", "$g.$g($v)");
    iff = if_("l($r) < 1", "A", "$r", "[ $v ]");
    write_function(def, asgn, iff, ret_(NULL));

    // Expand class
    cls("Expand", FALSE);
        def = def0_("at");
        iff = if_("l($a) < 2", "R", "[]");
        write_function(def, iff, ret_("s[1:]"));

        def = def_("$t", "Q");
        iff = if_("in_quotes", "IR", /*I:*/ "l($a) < 2", "R", "\"\"",
                                     /*R:*/ "\" \".j(s[1:])");
        write_function(def, iff, ret_("Expand.at()"));

        def = def0_("hash");
        write_function(def, ret_("l($a)-1"));

        def = def0_("dollar");
        write_function(def, ret_("os.getpid()"));

        write_unsupported_func0("exclamation", "$!");
        write_unsupported_func0("underbar", "$_");
        write_unsupported_func0("hyphen", "$-");

        def = def_("$m", "NV\'");
        iff = if_("D($n)", "R", "G($n)");
        write_function(def, iff, ret_("$v"));

        def = def_("eq", "NV\'");
        iff = if_("~D($n)", "LR", "S($n, $v)", "$v");
        write_function(def, iff, ret_("G($n)"));

        def = def_("qmark", "NV0");
        iff = if_("~D($n)", "Ir", /*I:*/ "$v $0|$v == \'\'", "A", "$v", "$Z", 
                                   /*r:*/ "$v");
        write_function(def, iff, ret_("G($n)"));

        def = def_("$p", "NV\'");
        iff = if_("~D($n)", "R", "''");
        write_function(def, iff, ret_("$v"));

        def = def_("$c$M", "NV\'");
        asgn = asgn_("$r", "G($n)");
        iff = if_("$r $0|$r $q", "A", "$r", "$v");
        write_function(def, asgn, iff, ret_("$r"));

        def = def_("$cEq", "NV\'");
        asgn = asgn_("$r", "G($n)");
        iff = if_("$r $0|$r $q", "LA", /*L:*/ "S($n, $v)", /*A:*/ "$r", "$v");
        write_function(def, asgn, iff, ret_("$r"));

        def = def_("$cQmark", "NV0");
        asgn = asgn_("$r", "G($n)");
        iff = if_("$r $0|$r $q", "I", "$v $0|$v $q", "Ar",
                       /*A:*/ "$v", "$Z", /*r:*/ "$v");
        write_function(def, asgn, iff, ret_("$r"));

        def = def_("$c$P", "NV\'");
        asgn = asgn_("$r", "G($n)");
        iff = if_("$r $0|$r $q", "R", "''");
        write_function(def, asgn, iff, ret_("$v"));

        write_unsupported_func("$f$T", TRUE, "${!$f*}");
        write_unsupported_func("$fAt", TRUE, "${!$f@}");
        write_unsupported_func("indices$T", TRUE, "${!$n[*]}");
        write_unsupported_func("indicesAt", TRUE, "${!$n[*]}");

    end_cls();

    cls("Bash2Py", FALSE);
        set_static(FALSE);

        write_function(asgn_("__slots__", "[\"$u\"]"));  // not ideal
        write_init_func("SV\'");

        def = def_("t()", "SV0");
        asgn = asgn_("$L", "$v");
        write_function(def, asgn, ret_("$v"));

        write_increment_func("preinc", "+");
        write_increment_func("postinc", "++");

        def = def_("isNull", "S");
        write_function(def, ret_("$L $0"));

        def = def_("notNullElse", "SV");
        iff = if_("$s.isNull()", "R", "$v");
        write_function(def, iff, ret_("$L"));

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

    end_cls();

    cleanup_dynamo();
}

/*************************************************************/

int main()
{
    run_test();
    return 0;
}

#endif // #ifdef TEST
#endif // #ifdef BASH2PY

