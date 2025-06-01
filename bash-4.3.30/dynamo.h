#include "burp.h"

extern char _EXCEPT[];

void init_dynamo(void);
void cleanup_dynamo(void);
void set_static(_BOOL is_static);
void write_assignment_func(char *name, char *oper);
void write_init_func(char *sig);
void write_increment_func(char *name, char *oper);
char *def_(char *name, char *sig);
char *def0_(char *name);
char *asgn_(char *l_name, char *r_name);
char *else_(char *types, ...);
char *if_(char *if_, char *types, ...);
char *ret_(char *return_what);
char *raise_(char *desc);
void cls(char *name, _BOOL is_exception);
void end_cls(void);
void write_function(char *first, ...);
void write_unsupported_func(char *name, _BOOL has_name_arg, char *bash_expr);
void write_unsupported_func0(char *name, char *bash_expr);

