/* compiler.h - shared data structures for all compiler phases.
 *
 * Pipeline:  source --lexer--> tokens --parser--> AST --semantic--> typed AST + symbol table
 *            --codegen--> quadruples (TAC) --optimizer--> optimized quadruples
 */
#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>

#define MAX_LEX 64
#define MAX_ADDR (MAX_LEX + 12)   /* a name in TAC: identifier plus ".<n>" suffix, or a constant */

/* ---------------- Phase 1: Lexical analysis ---------------- */
typedef enum {
    TK_INT, TK_FLOAT, TK_IF, TK_ELSE, TK_WHILE, TK_FOR,   /* keywords   */
    TK_RETURN,
    TK_ID, TK_INUM, TK_FNUM,                              /* names/nums */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,     /* operators  */
    TK_ASSIGN, TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,           /* separators */
    TK_SEMI, TK_COMMA,
    TK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    char lexeme[MAX_LEX];
    int line;
} Token;

extern Token *tokens;
extern int ntokens;

void lex(const char *src);
const char *token_category(TokenKind k);   /* "KEYWORD", "IDENTIFIER", ... */

/* ---------------- Phase 2: Syntax analysis (AST) ---------------- */
typedef enum {
    N_PROGRAM, N_BLOCK, N_DECL, N_ASSIGN, N_IF, N_WHILE, N_FOR,
    N_BINOP, N_NEG, N_NUM, N_ID, N_FUNC, N_RETURN
} NodeKind;

typedef enum { TY_NONE, TY_INT, TY_FLOAT, TY_ERROR } Type;

typedef struct Node {
    NodeKind kind;
    char text[MAX_LEX];    /* operator, name, number or type name */
    int line;
    Type type;             /* filled in by the semantic phase */
    struct Node *kid[4];   /* children; meaning depends on kind (see parser.c) */
    struct Node *next;     /* next item in a statement / declaration list */
    struct Symbol *sym;    /* Id nodes: the variable it refers to (semantic phase) */
} Node;

Node *parse(void);
const char *node_label(Node *n, char *buf);   /* "BinOp +", "Id a", ... */
const char *type_name(Type t);

/* ---------------- Phase 3: Semantic analysis ---------------- */
typedef struct Symbol {
    char name[MAX_LEX];
    char tac[MAX_ADDR];    /* name used in TAC: "x", or "x.3" when it shadows an earlier x */
    Type type;
    int size, offset, scope, line;
} Symbol;

extern Symbol symtab[];
extern int nsyms;

void analyze(Node *program);

/* ---------------- Phase 4: Intermediate code + optimization ---------------- */
typedef struct {
    char op[12];       /* + - * / % < > <= >= == != uminus = itof label goto iffalse return */
    char arg1[MAX_ADDR], arg2[MAX_ADDR], res[MAX_ADDR];
} Quad;

extern Quad code[], opt[];
extern int ncode, nopt;

void gen_program(Node *program);
void optimize(void);
void quad_to_tac(Quad *q, char *buf);   /* "t1 = a + b", "L1:", ... */

/* ---------------- Errors (shared by all phases) ---------------- */
typedef struct {
    const char *phase;
    int line;
    char msg[160];
} Error;

extern Error errors[];
extern int nerrors;

void error_at(const char *phase, int line, const char *fmt, ...);

/* ---------------- Output ---------------- */
void print_all(Node *ast);
void write_report(const char *path, const char *src, Node *ast);

#endif
