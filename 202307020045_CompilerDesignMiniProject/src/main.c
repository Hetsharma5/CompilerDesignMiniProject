/* main.c - driver: runs every phase in order and shows what each one produced.
 *
 *   minicc program.mc            compile a file
 *   minicc                       type a program, end with Ctrl+Z (Windows) / Ctrl+D
 *
 * Also writes report.html, a browser view of all phases.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

#define MAX_ERRORS 100

Error errors[MAX_ERRORS];
int nerrors;

void error_at(const char *phase, int line, const char *fmt, ...)
{
    if (nerrors == MAX_ERRORS) return;
    Error *e = &errors[nerrors++];
    va_list ap;
    va_start(ap, fmt);
    e->phase = phase;
    e->line = line;
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
}

static char *read_all(FILE *f)
{
    size_t len = 0, cap = 4096;
    char *buf = malloc(cap);
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 == cap) buf = realloc(buf, cap *= 2);
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return buf;
}

static void heading(const char *title)
{
    printf("\n==================== %s ====================\n", title);
}

static void print_tree(Node *n, const char *prefix, int last)
{
    char label[128], child_prefix[512];
    printf("%s%s%s", prefix, last ? "`-- " : "|-- ", node_label(n, label));
    if (n->type != TY_NONE) printf("  : %s", type_name(n->type));
    printf("\n");

    snprintf(child_prefix, sizeof child_prefix, "%s%s", prefix, last ? "    " : "|   ");
    Node *kids[64];
    int k = 0;
    for (int i = 0; i < 4; i++)
        for (Node *c = n->kid[i]; c && k < 64; c = c->next) kids[k++] = c;
    for (int i = 0; i < k; i++) print_tree(kids[i], child_prefix, i == k - 1);
}

static void print_code(Quad *q, int n)
{
    char tac[256];
    printf("  %-4s %-36s | %-8s %-8s %-8s %-8s\n", "#", "Three Address Code", "op", "arg1", "arg2", "result");
    for (int i = 0; i < n; i++) {
        quad_to_tac(&q[i], tac);
        printf("  %-4d %-36s | %-8s %-8s %-8s %-8s\n", i, tac, q[i].op, q[i].arg1, q[i].arg2, q[i].res);
    }
}

void print_all(Node *ast)
{
    heading("PHASE 1: LEXICAL ANALYSIS");
    printf("  %-5s %-12s %s\n", "Line", "Category", "Lexeme");
    for (int i = 0; i < ntokens; i++)
        printf("  %-5d %-12s %s\n", tokens[i].line, token_category(tokens[i].kind), tokens[i].lexeme);

    heading("PHASE 2: SYNTAX ANALYSIS (AST)");
    print_tree(ast, "  ", 1);

    heading("PHASE 3: SEMANTIC ANALYSIS (SYMBOL TABLE)");
    printf("  %-12s %-6s %-5s %-7s %-6s %s\n", "Name", "Type", "Size", "Offset", "Scope", "Line");
    for (int i = 0; i < nsyms; i++)
        printf("  %-12s %-6s %-5d %-7d %-6d %d\n", symtab[i].name, type_name(symtab[i].type),
               symtab[i].size, symtab[i].offset, symtab[i].scope, symtab[i].line);

    heading("PHASE 4: INTERMEDIATE CODE (QUADRUPLES)");
    print_code(code, ncode);

    heading("PHASE 5: OPTIMIZED CODE");
    print_code(opt, nopt);
    if (ncode) printf("\n  %d instructions -> %d instructions\n", ncode, nopt);

    heading("ERRORS");
    for (int i = 0; i < nerrors; i++)
        printf("  [%s] line %d: %s\n", errors[i].phase, errors[i].line, errors[i].msg);
    printf("  %d error(s)\n", nerrors);
}

int main(int argc, char **argv)
{
    FILE *f = stdin;
    if (argc > 1 && !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    if (f == stdin) printf("Type your program, then press Ctrl+Z and Enter (Ctrl+D on Linux):\n");
    char *src = read_all(f);
    if (f != stdin) fclose(f);

    lex(src);
    Node *ast = parse();
    /* later phases need a correct program, so stop at the first phase with errors */
    if (!nerrors) analyze(ast);
    if (!nerrors) { gen_program(ast); optimize(); }

    print_all(ast);
    write_report("report.html", src, ast);
    printf("\nReport written to report.html - open it in a browser.\n");
    return nerrors ? 1 : 0;
}
