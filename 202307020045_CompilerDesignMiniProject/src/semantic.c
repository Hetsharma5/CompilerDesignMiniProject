/* semantic.c - Phase 3: Semantic analysis.
 * Walks the AST, builds the symbol table (with block scopes) and checks:
 *   - variables are declared before use, and not declared twice in one scope
 *   - types: int/float mixing widens to float; float cannot be stored in int
 *   - '%' only works on ints
 * Every expression node gets its type written into node->type for codegen.
 */
#include <ctype.h>
#include <string.h>
#include "compiler.h"

#define MAX_SYMS 256

Symbol symtab[MAX_SYMS];
int nsyms;

static int active[MAX_SYMS];   /* 1 while the symbol's scope is open */
static int scope, offset;
static int in_main;   /* return is only allowed inside int main() */

static Symbol *lookup(const char *name)
{
    for (int i = nsyms - 1; i >= 0; i--)
        if (active[i] && !strcmp(symtab[i].name, name)) return &symtab[i];
    return NULL;
}

/* t1, t2, ... are the names codegen uses for temporaries */
static int is_temp_name(const char *s)
{
    if (s[0] != 't' || !s[1]) return 0;
    while (*++s) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void declare(Node *id, Type type)
{
    Symbol *old = lookup(id->text);
    if (old && old->scope == scope) {
        error_at("Semantic", id->line, "redeclaration of '%s' (first declared on line %d)",
                 id->text, old->line);
        return;
    }
    if (is_temp_name(id->text)) {
        error_at("Semantic", id->line, "'%s' is reserved for compiler temporaries", id->text);
        return;
    }
    if (nsyms == MAX_SYMS) {
        error_at("Semantic", id->line, "too many variables (max %d)", MAX_SYMS);
        return;
    }
    Symbol *s = &symtab[nsyms];
    strcpy(s->name, id->text);
    /* a later variable with an existing name (e.g. shadowing in a block) gets its own TAC name */
    strcpy(s->tac, id->text);
    for (int i = 0; i < nsyms; i++)
        if (!strcmp(symtab[i].name, id->text)) sprintf(s->tac, "%s.%d", id->text, nsyms);
    s->type = type;
    s->size = 4;
    s->offset = offset;
    s->scope = scope;
    s->line = id->line;
    active[nsyms++] = 1;
    offset += s->size;
    id->type = type;
    id->sym = s;
}

static Type check_expr(Node *n)
{
    switch (n->kind) {
    case N_NUM:
        n->type = strchr(n->text, '.') ? TY_FLOAT : TY_INT;
        break;
    case N_ID: {
        Symbol *s = lookup(n->text);
        if (!s) error_at("Semantic", n->line, "undeclared variable '%s'", n->text);
        n->type = s ? s->type : TY_ERROR;
        n->sym = s;
        break;
    }
    case N_NEG:
        n->type = check_expr(n->kid[0]);
        break;
    default: {   /* N_BINOP */
        Type l = check_expr(n->kid[0]), r = check_expr(n->kid[1]);
        if (l == TY_ERROR || r == TY_ERROR)
            n->type = TY_ERROR;   /* already reported; don't cascade */
        else if (!strcmp(n->text, "%") && (l == TY_FLOAT || r == TY_FLOAT)) {
            error_at("Semantic", n->line, "operator '%%' needs int operands, got %s %% %s",
                     type_name(l), type_name(r));
            n->type = TY_ERROR;
        } else if (strchr("<>=!", n->text[0]))
            n->type = TY_INT;     /* comparisons give 0 or 1 */
        else
            n->type = (l == TY_FLOAT || r == TY_FLOAT) ? TY_FLOAT : TY_INT;
    }
    }
    return n->type;
}

static void check_stmt(Node *n);

static void check_list(Node *n)
{
    for (; n; n = n->next) check_stmt(n);
}

static void check_stmt(Node *n)
{
    switch (n->kind) {
    case N_DECL: {
        Type t = !strcmp(n->text, "float") ? TY_FLOAT : TY_INT;
        for (Node *id = n->kid[0]; id; id = id->next) declare(id, t);
        break;
    }
    case N_ASSIGN: {
        Type l = check_expr(n->kid[0]), r = check_expr(n->kid[1]);
        if (l == TY_INT && r == TY_FLOAT)
            error_at("Semantic", n->line, "type mismatch: cannot assign float to int variable '%s'",
                     n->kid[0]->text);
        n->type = l;
        break;
    }
    case N_BLOCK:
        scope++;
        check_list(n->kid[0]);
        for (int i = 0; i < nsyms; i++)   /* close this scope */
            if (symtab[i].scope == scope) active[i] = 0;
        scope--;
        break;
    case N_IF:
        check_expr(n->kid[0]);
        check_stmt(n->kid[1]);
        if (n->kid[2]) check_stmt(n->kid[2]);
        break;
    case N_WHILE:
        check_expr(n->kid[0]);
        check_stmt(n->kid[1]);
        break;
    case N_FOR:
        check_stmt(n->kid[0]);
        check_expr(n->kid[1]);
        check_stmt(n->kid[2]);
        check_stmt(n->kid[3]);
        break;
    case N_FUNC:
        in_main = 1;
        check_stmt(n->kid[0]);
        in_main = 0;
        break;
    case N_RETURN:
        if (!in_main)
            error_at("Semantic", n->line, "'return' used outside of main()");
        if (check_expr(n->kid[0]) == TY_FLOAT)
            error_at("Semantic", n->line, "type mismatch: main() must return int, not float");
        break;
    default:
        break;
    }
}

void analyze(Node *program)
{
    check_list(program->kid[0]);
}
