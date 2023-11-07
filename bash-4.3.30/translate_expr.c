#include "config.h"

#ifdef BASH2PY

#include <stdio.h>
#include <setjmp.h>
#include <assert.h>
#include <ctype.h>
#include "burp.h"
#include "fix_string.h"

#include "bash2py_alloc.h"

/*
	evalexpr  := subexpr
	subexpr   := expcomma
	expcomma  := expassign *, exprassign*
	expassign := expcond [=|+=|-= ... ] expassign
	expcond   := explor [? expcomma : expcond ]
	explor    := expland * || expland  *
	expland   := expbor  * && expbor   *
	expbor    := expbxor * |  expbxor  *
	expbxor   := expband * ^   expband *
	expband   := exp5    * &   exp5    *
	exp5      := exp4 * [==|!=] exp4   *
	exp4      := expshift * [<|<=|>|>=] expshift *
	expshift  := exp3 * [<<|>>] exp3   *
	exp3      := exp2 * [+|-] exp2     *
	exp2      := exppower * [*|/|%] exppower *
	exppower  := exp1 * ** exppower *
	exp1      := [!|~|-|+] exp1 | exp0
	exp0      := [++|--] ARRAY  | ( expcomma ) | NUM | exppost 
	exppost   := ARRAY [ [++|--] ]
	array     := VAR [ [ expcomma ] ]
*/

typedef
enum nodeE {
	Number,
    Variable,
	Array,
	Makedefined,
	Makevalue,
	Makeclass,
    Preinc,
    Predec,
    Postinc,
    Postdec,
    Lnot,
    Bnot,
    Uminus,
	Power,
	Multiply,
	Divide,
	Mod,
	Plus,
	Minus,
    Lsh,
	Rsh,
	Lt,
	Le,
	Gt,
	Ge,
	Eq,
	Ne,
	Band,
	Bxor,
	Bor,
	Land,
	Lor,
	Cond,
    Assign,
    Multiply_assign,
    Divide_assign,
    Mod_assign,
    Plus_assign,
    Minus_assign,
    Lsh_assign,
    Rsh_assign,
    Band_assign,
    Bxor_assign,
    Bor_assign,
	Comma
} nodeE;

typedef
enum returnsE {
	Returns_number,
	Returns_variable,
	Returns_array,
	Returns_class
} returnsE;

typedef struct leafS {
	nodeE			m_type;
	returnsE		m_returns;
	struct nodeS	*m_chainP;
	struct nodeS	*m_firstChildP;
	struct nodeS	*m_nextSiblingP;
	char			*m_startP;
	char			*m_endP;
} leafT;

typedef struct nodeS {
	nodeE			m_type;
	returnsE		m_returns;
	struct nodeS	*m_chainP;
	struct nodeS	*m_firstChildP;
	struct nodeS	*m_nextSiblingP;
} nodeT;
  
static char		*g_stringP;
static char		*g_atP;
static burpT	g_expression  = {0,0,0,0,0,0};
static _BOOL	g_allow_array = FALSE;
static nodeT	*g_headP      = 0;
static jmp_buf  longbuf;

static void
skipspace(int lth)
{
	char	*endP;

	for (endP = g_atP + lth; *g_atP && g_atP < endP; ++g_atP);
	assert(g_atP == endP);
	for (; isspace(*g_atP); ++g_atP);
}

static void
delete_tree(void)
{
	nodeT	*nodeP;

	for (; nodeP = g_headP; ) {
		g_headP = nodeP->m_chainP;
		xfree(nodeP);
	}
}

static void
translation_error(char *messageP)
{
	char	*P;
	int		c;

	delete_tree();
	fputs(messageP, stderr);
	fputs(" at ", stderr);
	for (P = g_atP; c = *P; ++P) {
		if (c == '\n') {
			break;
		}
		if (c < 8) {
			continue;
		}
		fputc(c, stderr);
	}
	fputc('\n', stderr);

	longjmp (longbuf, 1);
	assert(0);
}

static leafT *
makeLeaf(nodeE type, char *startP, char *endP, returnsE returns)
{
	leafT	*leafP;

	leafP                 = (leafT *) xmalloc(sizeof(leafT));
	if (!leafP) {
		translation_error("Can't make leaf");
	}
	leafP->m_chainP       = g_headP;
	g_headP               = (nodeT *) leafP;
	leafP->m_type         = type;
	leafP->m_returns      = returns;
	leafP->m_firstChildP  = NULL;
	leafP->m_nextSiblingP = NULL;
	leafP->m_startP       = startP;
	leafP->m_endP         = endP;
	return leafP;
}

static nodeT *
makeNode(nodeE type, nodeT *childP, returnsE returns)
{
	nodeT	*nodeP;

	nodeP                 = (nodeT *) xmalloc(sizeof(nodeT));
	if (!nodeP) {
		translation_error("Can't make node");
	}
	nodeP->m_chainP       = g_headP;
	g_headP               = nodeP;
	nodeP->m_type         = type;
	nodeP->m_returns      = returns;
	nodeP->m_firstChildP  = childP;
	nodeP->m_nextSiblingP = NULL;
	return nodeP;
}

static nodeT *
makeClass(nodeT *nodeP)
{
	switch (nodeP->m_returns) {
	case Returns_class:
	case Returns_array:
		break;
	case Returns_variable:
	{
		nodeT	*childP;
		childP = nodeP;
		nodeP  = makeNode(Makeclass, childP, Returns_class);
		break;
	}
	default:
		translation_error("Can't make class from value");
		return NULL;
	}
	return (nodeP);
}

static nodeT *
makeValue(nodeT *nodeP)
{
	switch (nodeP->m_returns) {
	case Returns_class:
	case Returns_variable:
	{
		nodeT	*childP;

		childP = nodeP;
		nodeP  = makeNode(Makevalue, childP, Returns_number);
	}}
	return (nodeP);
}

static nodeT *translate_comma(void);

// Number

static nodeT *
translate_number(void)
{
	leafT	*leafP;
	char	*P;
	int		c;

	if (*g_atP == '0') {
		if (g_atP[1] == 'x' || g_atP[1] == 'X') {
			for (P = g_atP + 2; c = *P; ++P) {
				if ((c < '0' || '9' < c) &&
					(c < 'A' || 'F' < c) &&
					(c < 'a' || 'f' < c)) {
					break;
			}	}
		} else {
			for (P = g_atP + 1; c = *P; ++P) {
				if (c < '0' || '7' < c) {
					break;
			}	}
		}
	} else {
		for (P = g_atP; c = *P; ++P) {
			if (c < '0' || '9' < c) {
				break;
	}	}	}
	if (P == g_atP) {
		leafP = 0;
	} else {
		leafP = makeLeaf(Number, g_atP, P, Returns_number);
		g_atP = P;
		skipspace(0);
	} 
	return (nodeT *) leafP;
}

// Variable

static nodeT *
translate_variable(void)
{
	char	*P;
	int		c;
	leafT	*leafP;

	if (*g_atP == START_EXPAND) {
		P = endExpand(g_atP);
		if (!P) {
			translation_error("No end of expansion");
		}
	} else {
		P = g_atP;
		if ((c = *P) == '_' || isalpha(c)) {
			while ( (c = *++P) == '_' || isalnum(c) );
		}
		if (P == g_atP) {
			translation_error("Didn't find variable");
	}	}
	leafP = makeLeaf(Variable, g_atP, P, Returns_variable);
	g_atP = P;
	skipspace(0);
	return (nodeT *) leafP;
}

static nodeT *
translate_array(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP = translate_variable();
	if (g_allow_array && *g_atP == '[') {
		skipspace(1);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Array, childP, Returns_array);
		// The subscript
		nextP  = translate_comma();
		childP->m_nextSiblingP = makeValue(nextP);
		if (*g_atP != ']') {
			translation_error("Subscript not terminated with ']'");
		}
		skipspace(1);
	}
	return nodeP;
}

// exppost   := ARRAY [ [++|--] ]

static nodeT *
translate_post(void)
{
	nodeT	*nodeP, *childP;
	nodeE	type;
	int		lth;

	nodeP = translate_array();

	lth = 0;
	switch (*g_atP) {
	case '+':
		if (g_atP[1] == '+') {
			type = Postinc;
			lth = 2;
		}
		break;
	case '-':
		if (g_atP[1] == '-') {
			type = Postdec;
			lth  = 2;
		}
		break;
	}
	if (lth) {
		skipspace(lth);
		nodeP  = makeClass(nodeP);
		childP = nodeP;
		nodeP  = makeNode(type, childP, Returns_class);
	}
	return (nodeP);
}

// exp0      := [++|--] ARRAY  | ( expcomma ) | NUM | exppost 

static nodeT *
translate_exp0(void)
{
	nodeT	*nodeP, *childP;
	nodeE	type;
	int		lth;

	nodeP = translate_number();
	if (nodeP) {
		return nodeP;
	}

	lth = 0;
	switch (*g_atP) {
	case '(':
		skipspace(1);
		nodeP = translate_comma();
		skipspace(0);
		if (*g_atP != ')') {
			translation_error("Open ( not matched with close )");
		}
		skipspace(1);
		return nodeP;
	case '+':
		switch (g_atP[1]) {
		case '+':
			type = Preinc;
			lth  = 2;
		}
		break;
	case '-':
		switch (g_atP[1]) {
		case '-':
			type = Predec;
			lth  = 2;
		}
		break;
	}
	if (!lth) {
		nodeP = translate_post();
	} else {
		skipspace(lth);
		childP = translate_array();
		childP = makeClass(childP);
		nodeP  = makeNode(type, childP, Returns_class);
	}
	return nodeP;
}

// exp1      := [!|~|-|+] exp1 | exp0

static nodeT *
translate_unary(void)
{
	nodeT	*nodeP, *childP;
	nodeE	type;
	int		lth;

retry:
	lth = 0;
	switch (*g_atP) {
	case '+':
		switch (g_atP[1]) {
		case '+':
		case '=':
			break;
		default:
			// Just discard unary plus
			skipspace(1);
			goto retry;
		}
		break;
	case '-':
		switch (g_atP[1]) {
		case '-':
		case '=':
			break;
		default:
			type = Uminus;
			lth  = 1;
		}
		break;
	case '!':
		if (g_atP[1] != '=') {
			type = Lnot;
			lth  = 1;
		}
		break;
	case '~':
		if (g_atP[1] != '=') {
			type = Bnot;
			lth  = 1;
		}
		break;
	}
	if (!lth) {
		nodeP  = translate_exp0();
	} else {
		skipspace(lth);
		childP = translate_unary();
		childP = makeValue(childP);
		nodeP  = makeNode(type, childP, Returns_number);
	}
	return nodeP;
}

// exppower  := exp1 * ** exppower *
// 2 ** 3 ** 2 == 2 ** (3 ** 2) = 512  -- not (2 ** 3) ** 2 = 64

static nodeT *
translate_power(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP = translate_unary();
	if (*g_atP == '*' && g_atP[1] == '*') {
		skipspace(2);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Power, childP, Returns_number);
		nextP = translate_power();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// exp2      := exppower * [*|/|%] exppower *

static nodeT *
translate_multiply(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_power();
	lth = 0;
	switch (*g_atP) {
	case '*':
		switch (g_atP[1]) {
		case '*':
		case '=':
			break;
		default:
			type = Multiply;
			lth  = 1;
		}
		break;
	case '/':
		switch (g_atP[1]) {
		case '=':
			break;
		default:
			type = Divide;
			lth  = 1;
		}
		break;
	case '%':
		switch (g_atP[1]) {
		case '=':
			break;
		default:
			type = Mod;
			lth  = 1;
		}
		break;
	}
	if (lth) {
		skipspace(lth);
		childP = makeValue(nodeP);
		nodeP  = makeNode(type, childP, Returns_number);
		nextP  = translate_multiply();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// exp3      := exp2 * [+|-] exp2     *

static nodeT *
translate_plus(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_multiply();
	lth = 0;
	switch (*g_atP) {
	case '+':
		switch (g_atP[1]) {
		case '+':
		case '=':
			break;
		default:
			type = Plus;
			lth  = 1;
		}
		break;
	case '-':
		switch (g_atP[1]) {
		case '-':
		case '=':
			break;
		default:
			type = Minus;
			lth  = 1;
	}	}
	if (lth) {
		skipspace(lth);
		childP = makeValue(nodeP);
		nodeP  = makeNode(type, childP, Returns_number);
		nextP  = translate_plus();
		childP->m_nextSiblingP = makeValue(nextP);
	}	
	return nodeP;
}

// expshift  := exp3 * [<<|>>] exp3   *

static nodeT *
translate_shift(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_plus();
	lth = 0;
	switch (*g_atP) {
	case '<':
		if (g_atP[1] == '<') {
			type = Lsh;
			lth  = 2;
		}
		break;
	case '>':
		if (g_atP[1] == '>') {
			type = Rsh;
			lth  = 2;
		}
		break;
	}
	if (lth) {
		skipspace(lth);
		childP = makeValue(nodeP);
		nodeP  = makeNode(type, childP, Returns_number);
		nextP  = translate_shift();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// exp4      := expshift * [<|<=|>|>=] expshift *

static nodeT *
translate_exp4(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_shift();
	lth = 0;
	switch (*g_atP) {
	case '<':
		switch (g_atP[1]) {
		case '=':
			type = Le;
			lth  = 2;
			break;
		case '<':
			break;
		default:
			type = Lt;
			lth  = 1;
		}
		break;
	case '>':
		switch (g_atP[1]) {
		case '=':
			type = Ge;
			lth  = 2;
			break;
		case '>':
			break;
		default:
			type = Gt;
			lth  = 1;
		}
		break;
	}
	if (lth) {
		skipspace(lth);
		childP = makeValue(nodeP);
		nodeP  = makeNode(type, childP, Returns_number);
		nextP  = translate_exp4();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// exp5      := exp4 * [==|!=] exp4   *

static nodeT *
translate_exp5(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_exp4();
	lth = 0;
	if (*g_atP == '=' && g_atP[1] == '=') {
		type = Eq;
		lth = 2;
	} else if (*g_atP == '!' && g_atP[1] == '=') {
		type = Ne;
		lth  = 2;
	}
	if (lth) {
		skipspace(lth);
		childP = makeValue(nodeP);
		nodeP  = makeNode(type, childP, Returns_number);
		nextP  = translate_exp5();
		childP->m_nextSiblingP = makeValue(nextP);
	}	
	return nodeP;
}

// expband   := exp5    * &   exp5    *

static nodeT *
translate_band(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_exp5();
	if (*g_atP == '&' && (g_atP[1] != '&' && g_atP[1] != '=')) {
		skipspace(1);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Band, childP, Returns_number);
		nextP  = translate_band();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expbxor   := expband * ^   expband *

static nodeT *
translate_bxor(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_band();
	if (*g_atP == '^' && g_atP[1] != '=') {
		skipspace(1);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Bxor, childP, Returns_number);
		nextP  = translate_bxor();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expbor    := expbxor * |  expbxor  *

static nodeT *
translate_bor(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_bxor();
	if (*g_atP == '|' && (g_atP[1] != '|' && g_atP[1] != '=')) {
		skipspace(1);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Bor, childP, Returns_number);
		nextP  = translate_bor();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expland   := expbor  * && expbor   *

static nodeT *
translate_land(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_bor();
	if (*g_atP == '&' && g_atP[1] == '&') {
		skipspace(2);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Land, childP, Returns_number);
		nextP  = translate_land();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// explor    := expland * || expland  *

static nodeT *
translate_lor(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_land();
	if (*g_atP == '|' && g_atP[1] == '|') {
		skipspace(2);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Lor, childP, Returns_number);
		nextP  = translate_lor();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expcond   := explor [? expcomma : expcond ]

static nodeT *
translate_cond(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP = translate_lor();
	if (*g_atP == '?') {
		skipspace(1);
		childP = makeValue(nodeP);
		nodeP  = makeNode(Cond, childP, Returns_number);
		nextP  = translate_comma();
		nextP  = makeValue(nextP);
		childP->m_nextSiblingP = nextP;
		childP = nextP;
		if (*g_atP != ':') {
			translation_error(" .. ? .. not followed by :");
		}
		skipspace(1);
		nextP = translate_cond();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expassign := expcond [=|+=|-= ... ] expassign

static nodeT *
translate_assign(void)
{
	nodeT	*nodeP, *childP, *nextP;
	nodeE	type;
	int		lth;

	nodeP = translate_cond();
	lth = 0;
	switch (*g_atP) {
	case 0:
		break;
	case '=':
		if (g_atP[1] != '=') {
			type = Assign;
			lth  = 1;
		}
		break;
	case '<':
		if (g_atP[1] == '<' && g_atP[2] == '=') {
			type = Lsh_assign;
			lth  = 3;
		}
		break;
	case '>':
		if (g_atP[1] == '>' && g_atP[2] == '=') {
			type = Rsh_assign;
			lth  = 3;
		}
		break;
	default:
		if (g_atP[1] != '=') {
			break;
		}
		lth = 2;
		switch (*g_atP) {
		case '*':
			type = Multiply_assign;
			break;
		case '/':
			type = Divide_assign;
			break;
		case '%':
			type = Mod_assign;
			break;
		case '+':
			type = Plus_assign;
			break;
		case '-':
			type = Minus_assign;
			break;
		case '&':
			type = Band_assign;
			break;
		case '^':
			type = Bxor_assign;
			break;
		case '|':
			type = Bor_assign;
			break;
		default:
			lth = 0;
	}	}
	if (lth) {
		skipspace(lth);
		childP = makeClass(nodeP);
		nodeP  = makeNode(type, childP, Returns_class);
		nextP  = translate_assign();
		childP->m_nextSiblingP = makeValue(nextP);
	}
	return nodeP;
}

// expcomma  := expassign *, exprassign*

static nodeT *
translate_comma(void)
{
	nodeT	*nodeP, *childP, *nextP;

	nodeP  = translate_assign();
	childP = 0;
	while (*g_atP == ',') {
		skipspace(1);
		if (!childP) {
			childP = makeValue(nodeP);
			nodeP  = makeNode(Comma, childP, Returns_number);
		}
		nextP = translate_assign();
		nextP = makeValue(nextP);
		childP->m_nextSiblingP = nextP;
		childP = nextP;
	}
	return nodeP;
}

/*
    evalexpr  := subexpr
    subexpr   := expcomma
    expcomma  := expassign *, exprassign*
    expassign := expcond [=|+=|-= ... ] expassign
    expcond   := explor [? expcomma : expcond ]
    explor    := expland * || expland  *
    expland   := expbor  * && expbor   *
    expbor    := expbxor * |  expbxor  *
    expbxor   := expband * ^   expband *
    expband   := exp5    * &   exp5    *
    exp5      := exp4 * [==|!=] exp4   *
    exp4      := expshift * [<|<=|>|>=] expshift *
    expshift  := add * [<<|>>] add     *
    add       := multiply * [+|-] multiply   *
    multiply  := exppower * [*|/|%] exppower *
    exppower  := unary * ** exppower *
    unary     := [!|~|-|+] exp1 | exp0
    exp0      := [++|--] ARRAY  | ( expcomma ) | NUM | exppost 
    exppost   := ARRAY [ [++|--] ]
    array     := VAR [ [ expcomma ] ]
*/

#define EMITN(S, L)   burpn(&g_expression, S, L)
#define EMITS(S)      burps(&g_expression, S)
#define EMITC(C)      burpc(&g_expression, C)

extern translateT	g_translate;

static void
print_leaf(leafT *leafP)
{
	EMITN(leafP->m_startP, leafP->m_endP - leafP->m_startP);
	return;
}

static void
print_translation(nodeT *nodeP)
{
	char *textP;

	assert(nodeP);

	switch (nodeP->m_type) {
	case Number:
	case Variable:
		print_leaf((leafT *) nodeP);
		return;
	case Array:
	{
		nodeT	*childP;

		childP = nodeP->m_firstChildP;
		print_translation(childP);
		EMITC('[');
		childP = childP->m_nextSiblingP;
		print_translation(childP);
		EMITC(']');
		break;
	}
	case Makeclass:
		g_translate.m_function.m_make = TRUE;
		EMITS("Make(\"");
		print_translation(nodeP->m_firstChildP);
		EMITS("\")");
		break;
	case Makevalue:
		g_translate.m_value.m_uses = TRUE;
		textP = ".val";
		goto suffix;
	case Preinc:
		g_translate.m_value.m_preincrement = TRUE;
		textP = ".preinc()";
		goto suffix;
	case Predec:
		g_translate.m_value.m_preincrement = TRUE;
		textP = ".preinc(-1)";
		goto suffix;
	case Postinc:
		g_translate.m_value.m_postincrement = TRUE;
		textP = ".postinc()";
		goto suffix;
	case Postdec:
		g_translate.m_value.m_postincrement = TRUE;
		textP = ".postinc(-1)";
suffix:
		print_translation(nodeP->m_firstChildP);
		EMITS(textP);
		break;
	case Lnot:
		textP = "!";
		goto prefix;
	case Bnot:
		textP = "~";
		goto prefix;
	case Uminus:
		textP = "-";
prefix:
		EMITS(textP);
		print_translation(nodeP->m_firstChildP);
		break;
	case Power:
		textP = " ** ";
		goto binary;
	case Multiply:
		textP = " * ";
		goto binary;
	case Divide:
		textP = " // ";
		goto binary;
	case Mod:
		textP = " % ";
		goto binary;
	case Plus:
		textP = " + ";
		goto binary;
	case Minus:
		textP = " - ";
		goto binary;
	case Lsh:
		textP = " << ";
		goto binary;
	case Rsh:
		textP = " >> ";
		goto binary;
	case Lt:
		textP = " < ";
		goto binary;
	case Le:
		textP = " <= ";
		goto binary;
	case Gt:
		textP = " > ";
		goto binary;
	case Ge:
		textP = " >= ";
		goto binary;
	case Eq:
		textP = " == ";
		goto binary;
	case Ne:
		textP = " != ";
		goto binary;
	case Band:
		textP = " & ";
		goto binary;
	case Bxor:
		textP = " ^ ";
		goto binary;
	case Bor:
		textP = " | ";
		goto binary;
	case Land:
		textP = " && ";
		goto binary;
	case Lor:
		textP = " || ";
binary:
		{
			nodeT	*childP;
	
			EMITC('(');
			childP = nodeP->m_firstChildP;
			print_translation(childP);
			EMITS(textP);
			print_translation(childP->m_nextSiblingP);
			EMITC(')');
		}
		break;
	case Cond:
		{
			nodeT	*childP, *secondP;
	
			childP = nodeP->m_firstChildP;
			EMITC('(');
			secondP = childP->m_nextSiblingP;
			print_translation(secondP);
			EMITS(" if ");
			print_translation(childP);
			EMITS(" else ");
			print_translation(secondP->m_nextSiblingP);
			EMITC(')');
		}
		break;
	case Assign:
		g_translate.m_value.m_set_value = TRUE;
		textP = "setValue";
		goto assign;
	case Multiply_assign:
		g_translate.m_value.m_multiply = TRUE;
		textP = "multiply";
		goto assign;
	case Divide_assign:
		g_translate.m_value.m_idivide = TRUE;
		textP = "idivide";
		goto assign;
	case Mod_assign:
		g_translate.m_value.m_mod = TRUE;
		textP = "mod";
		goto assign;
	case Plus_assign:
		g_translate.m_value.m_plus = TRUE;
		textP = "plus";
		goto assign;
	case Minus_assign:
		g_translate.m_value.m_minus = TRUE;
		textP = "minus";
		goto assign;
	case Lsh_assign:
		g_translate.m_value.m_lsh = TRUE;
		textP = "lsh";
		goto assign;
	case Rsh_assign:
		g_translate.m_value.m_rsh = TRUE;
		textP = "rsh";
		goto assign;
	case Band_assign:
		g_translate.m_value.m_band = TRUE;
		textP = "band";
		goto assign;
	case Bxor_assign:
		g_translate.m_value.m_bxor = TRUE;
		textP = "bxor";
		goto assign;
	case Bor_assign:
		g_translate.m_value.m_bor = TRUE;
		textP = "bor";
assign:
		{
			nodeT	*childP;
	
			childP = nodeP->m_firstChildP;
			print_translation(childP);
			EMITC('.');
			EMITS(textP);
			EMITC('(');
			print_translation(childP->m_nextSiblingP);
			EMITC(')');
		}
		break;
	case Comma:
		{
			nodeT	*childP, *nextP;
	
			childP = nodeP->m_firstChildP;
			EMITC('(');
			for (; nextP = childP->m_nextSiblingP; childP = nextP) {
				EMITC('(');
				print_translation(childP);
				EMITS(")*0+");
			}
			EMITC('(');
			print_translation(childP);
			EMITS("))");
		}
		break;
	
	default:
		assert(0);
	}
	return;
}


/* External entry point 
 * Returns where the end of the translation was
 */


_BOOL translate_expression(char *stringP, char **translationPP, _BOOL allow_array)
{
	nodeT	*treeP;
	int 	ret;

	g_stringP = g_atP = stringP;
	skipspace(0);

	g_allow_array   = allow_array;

 	ret = setjmp (longbuf);
	if (ret) {
		*translationPP = '\0';
		return FALSE;
	}

	g_headP = NULL;
  	treeP   = translate_comma();
	assert(treeP);
  
  	g_expression.m_lth = 0;
	print_translation(treeP);

	delete_tree();
  
  	if (!g_expression.m_P) {
    	// Bizarre
    	EMITC(0);
  	}

  	*translationPP = g_expression.m_P;
	return TRUE;
}

#ifdef TEST
#include <string.h>

int
main(int argc, char **argv)
{
	char		*bufferP   = 0;
	size_t		buffer_lth = 0;
	char		*translationP;
	int			lth;

	while(0 <= getline(&bufferP, &buffer_lth, stdin)) {
		lth = strlen(bufferP);
		if (lth && bufferP[lth-1] == '\n') {
			bufferP[lth-1] = 0;
		}
		printf("> %s\n", bufferP);
		if (!translate_expression(bufferP, &translationP, TRUE)) {
			printf("Can't translate\n");
			continue;
		}
		printf("< %s\n", translationP);
	}
	return(0);
}
#endif

#endif

