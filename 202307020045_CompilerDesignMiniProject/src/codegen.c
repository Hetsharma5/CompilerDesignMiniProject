/* codegen.c - Phase 4: Intermediate code generation and optimization.
 *
 * gen_program(): walks the typed AST and emits three-address code, stored as
 *   quadruples (op, arg1, arg2, result). int values used in float context get an
 *   explicit 'itof' conversion. Control flow uses labels + goto/ifFalse:
 *
 *     if (c) S1 else S2          while (c) S            for (i; c; s) S
 *       t = c                      L1: t = c              i
 *       ifFalse t goto L1              ifFalse t goto L2  L1: t = c
 *       S1                             S                      ifFalse t goto L2
 *       goto L2                        goto L1                S ; s
 *     L1: S2                       L2:                        goto L1
 *     L2:                                                 L2:
 *
 * optimize(): copies code[] into opt[] applying, inside each basic block,
 *   - constant propagation  (x = 5; y = x + 1   ->  y = 5 + 1)
 *   - constant folding      (y = 5 + 1          ->  y = 6)
 *   - branch folding        (ifFalse 1 goto L   ->  removed, ifFalse 0 -> goto)
 *   - dead temporary removal (t3 = 6 never used ->  removed)
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

#define MAX_CODE 2048

Quad code[MAX_CODE], opt[MAX_CODE];
int ncode, nopt;
static int ntemps, nlabels;

static void emit(const char *op, const char *a1, const char *a2, const char *res)
{
    if (ncode == MAX_CODE) {
        error_at("Codegen", 0, "program too large (max %d quadruples)", MAX_CODE);
        exit(1);
    }
    Quad *q = &code[ncode++];
    strcpy(q->op, op);
    strcpy(q->arg1, a1);
    strcpy(q->arg2, a2);
    strcpy(q->res, res);
}

static void new_temp(char *buf)  { sprintf(buf, "t%d", ++ntemps); }
static void new_label(char *buf) { sprintf(buf, "L%d", ++nlabels); }

/* int value in a float context: emit t = (float) x and use t instead */
static void widen(char *addr, Type from)
{
    if (from != TY_INT) return;
    char t[MAX_ADDR];
    new_temp(t);
    emit("itof", addr, "", t);
    strcpy(addr, t);
}

/* Emits code for expression n and writes the name holding its value into out. */
static void gen_expr(Node *n, char *out)
{
    char l[MAX_ADDR], r[MAX_ADDR];
    switch (n->kind) {
    case N_NUM:
        strcpy(out, n->text);
        break;
    case N_ID:
        strcpy(out, n->sym->tac);
        break;
    case N_NEG:
        gen_expr(n->kid[0], l);
        new_temp(out);
        emit("uminus", l, "", out);
        break;
    default:   /* N_BINOP */
        gen_expr(n->kid[0], l);
        gen_expr(n->kid[1], r);
        if (n->kid[0]->type == TY_FLOAT || n->kid[1]->type == TY_FLOAT) {
            widen(l, n->kid[0]->type);
            widen(r, n->kid[1]->type);
        }
        new_temp(out);
        emit(n->text, l, r, out);
    }
}

static void gen_stmt(Node *n);

static void gen_list(Node *n)
{
    for (; n; n = n->next) gen_stmt(n);
}

static void gen_stmt(Node *n)
{
    char v[MAX_ADDR], l1[MAX_ADDR], l2[MAX_ADDR];
    switch (n->kind) {
    case N_ASSIGN:
        gen_expr(n->kid[1], v);
        if (n->kid[0]->type == TY_FLOAT) widen(v, n->kid[1]->type);
        emit("=", v, "", n->kid[0]->sym->tac);
        break;
    case N_BLOCK:
        gen_list(n->kid[0]);
        break;
    case N_IF:
        new_label(l1);
        gen_expr(n->kid[0], v);
        emit("iffalse", v, "", l1);
        gen_stmt(n->kid[1]);
        if (n->kid[2]) {
            new_label(l2);
            emit("goto", "", "", l2);
            emit("label", "", "", l1);
            gen_stmt(n->kid[2]);
            emit("label", "", "", l2);
        } else {
            emit("label", "", "", l1);
        }
        break;
    case N_WHILE:
        new_label(l1);
        new_label(l2);
        emit("label", "", "", l1);
        gen_expr(n->kid[0], v);
        emit("iffalse", v, "", l2);
        gen_stmt(n->kid[1]);
        emit("goto", "", "", l1);
        emit("label", "", "", l2);
        break;
    case N_FOR:
        new_label(l1);
        new_label(l2);
        gen_stmt(n->kid[0]);
        emit("label", "", "", l1);
        gen_expr(n->kid[1], v);
        emit("iffalse", v, "", l2);
        gen_stmt(n->kid[3]);
        gen_stmt(n->kid[2]);
        emit("goto", "", "", l1);
        emit("label", "", "", l2);
        break;
    case N_FUNC:
        emit("label", "", "", n->text);
        gen_stmt(n->kid[0]);
        break;
    case N_RETURN:
        gen_expr(n->kid[0], v);
        emit("return", v, "", "");
        break;
    default:   /* declarations produce no code */
        break;
    }
}

void gen_program(Node *program)
{
    gen_list(program->kid[0]);
}

void quad_to_tac(Quad *q, char *buf)
{
    if (!strcmp(q->op, "label"))        sprintf(buf, "%s:", q->res);
    else if (!strcmp(q->op, "goto"))    sprintf(buf, "    goto %s", q->res);
    else if (!strcmp(q->op, "iffalse")) sprintf(buf, "    ifFalse %s goto %s", q->arg1, q->res);
    else if (!strcmp(q->op, "="))       sprintf(buf, "    %s = %s", q->res, q->arg1);
    else if (!strcmp(q->op, "uminus"))  sprintf(buf, "    %s = - %s", q->res, q->arg1);
    else if (!strcmp(q->op, "itof"))    sprintf(buf, "    %s = (float) %s", q->res, q->arg1);
    else if (!strcmp(q->op, "return"))  sprintf(buf, "    return %s", q->arg1);
    else                                sprintf(buf, "    %s = %s %s %s", q->res, q->arg1, q->op, q->arg2);
}

/* ---------------- Optimizer ---------------- */

static struct { char name[MAX_ADDR], val[MAX_ADDR]; } known[MAX_CODE];   /* name -> constant */
static int nknown;

static char *known_value(const char *name)
{
    for (int i = 0; i < nknown; i++)
        if (!strcmp(known[i].name, name)) return known[i].val;
    return NULL;
}

static void forget(const char *name)
{
    for (int i = 0; i < nknown; i++)
        if (!strcmp(known[i].name, name)) { known[i] = known[--nknown]; return; }
}

static void remember(const char *name, const char *val)
{
    forget(name);
    strcpy(known[nknown].name, name);
    strcpy(known[nknown++].val, val);
}

static int is_const(const char *s)
{
    return isdigit((unsigned char)s[0]) || (s[0] == '-' && isdigit((unsigned char)s[1]));
}

static int is_float_const(const char *s) { return strpbrk(s, ".eE") != NULL; }

/* t1, t2, ... (user variables can't use these names, see semantic.c) */
static int is_temp(const char *s)
{
    if (s[0] != 't' || !s[1]) return 0;
    while (*++s) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void format_float(double d, char *out)
{
    sprintf(out, "%g", d);
    if (!strpbrk(out, ".eE")) strcat(out, ".0");
}

/* Computes "a op b" for constants a and b. Returns 0 if it can't (e.g. x / 0). */
static int fold(const char *op, const char *a, const char *b, char *out)
{
    if (is_float_const(a) || is_float_const(b)) {
        double x = atof(a), y = atof(b), r;
        if      (!strcmp(op, "+"))  r = x + y;
        else if (!strcmp(op, "-"))  r = x - y;
        else if (!strcmp(op, "*"))  r = x * y;
        else if (!strcmp(op, "/"))  { if (y == 0) return 0; r = x / y; }
        else {
            int c = !strcmp(op, "<") ? x < y : !strcmp(op, ">") ? x > y :
                    !strcmp(op, "<=") ? x <= y : !strcmp(op, ">=") ? x >= y :
                    !strcmp(op, "==") ? x == y : !strcmp(op, "!=") ? x != y : -1;
            if (c < 0) return 0;
            sprintf(out, "%d", c);
            return 1;
        }
        format_float(r, out);
        return 1;
    }
    long x = atol(a), y = atol(b), r;
    if      (!strcmp(op, "+"))  r = x + y;
    else if (!strcmp(op, "-"))  r = x - y;
    else if (!strcmp(op, "*"))  r = x * y;
    else if (!strcmp(op, "/"))  { if (y == 0) return 0; r = x / y; }
    else if (!strcmp(op, "%"))  { if (y == 0) return 0; r = x % y; }
    else if (!strcmp(op, "<"))  r = x < y;
    else if (!strcmp(op, ">"))  r = x > y;
    else if (!strcmp(op, "<=")) r = x <= y;
    else if (!strcmp(op, ">=")) r = x >= y;
    else if (!strcmp(op, "==")) r = x == y;
    else if (!strcmp(op, "!=")) r = x != y;
    else return 0;
    sprintf(out, "%ld", r);
    return 1;
}

static void make_copy(Quad *q, const char *val)
{
    strcpy(q->op, "=");
    strcpy(q->arg1, val);
    q->arg2[0] = '\0';
}

void optimize(void)
{
    nopt = nknown = 0;
    for (int i = 0; i < ncode; i++) {
        Quad q = code[i];
        char *v, folded[MAX_ADDR];

        if (!strcmp(q.op, "label")) {
            nknown = 0;   /* new basic block: other paths may reach here */
            opt[nopt++] = q;
            continue;
        }
        if (!strcmp(q.op, "goto")) { opt[nopt++] = q; continue; }

        /* constant propagation: replace operands with known values */
        if ((v = known_value(q.arg1))) strcpy(q.arg1, v);
        if ((v = known_value(q.arg2))) strcpy(q.arg2, v);

        if (!strcmp(q.op, "iffalse")) {
            if (is_const(q.arg1)) {
                if (atof(q.arg1) != 0) continue;   /* condition always true: never jumps */
                strcpy(q.op, "goto");               /* always false: always jumps */
                q.arg1[0] = '\0';
            }
            opt[nopt++] = q;
            continue;
        }

        /* constant folding */
        if (!strcmp(q.op, "itof") && is_const(q.arg1)) {
            format_float(atof(q.arg1), folded);
            make_copy(&q, folded);
        } else if (!strcmp(q.op, "uminus") && is_const(q.arg1)) {
            if (fold("-", is_float_const(q.arg1) ? "0.0" : "0", q.arg1, folded)) make_copy(&q, folded);
        } else if (q.arg2[0] && is_const(q.arg1) && is_const(q.arg2)) {
            if (fold(q.op, q.arg1, q.arg2, folded)) make_copy(&q, folded);
        }

        if (!strcmp(q.op, "=") && is_const(q.arg1)) remember(q.res, q.arg1);
        else forget(q.res);
        opt[nopt++] = q;
    }

    /* dead temporary removal: drop "tN = const" when tN is no longer used */
    int n = 0;
    for (int i = 0; i < nopt; i++) {
        Quad *q = &opt[i];
        int dead = !strcmp(q->op, "=") && is_temp(q->res) && is_const(q->arg1);
        for (int j = 0; dead && j < nopt; j++)
            if (!strcmp(opt[j].arg1, q->res) || !strcmp(opt[j].arg2, q->res)) dead = 0;
        if (!dead) opt[n++] = *q;
    }
    nopt = n;
}
