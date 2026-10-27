/* parser.c - Phase 2: Syntax analysis.
 * Recursive descent parser: one C function per grammar rule. Builds an AST.
 *
 *   program → 'int' 'main' '(' ')' block EOF  |  stmt* EOF
 *   stmt    → decl | assign ';' | if | while | for | block | return
 *   return  → 'return' expr ';'
 *   decl    → ('int' | 'float') ID (',' ID)* ';'
 *   assign  → ID '=' expr
 *   if      → 'if' '(' expr ')' stmt [ 'else' stmt ]
 *   while   → 'while' '(' expr ')' stmt
 *   for     → 'for' '(' assign ';' expr ';' assign ')' stmt
 *   block   → '{' stmt* '}'
 *   expr    → add [ ('<'|'>'|'<='|'>='|'=='|'!=') add ]
 *   add     → mul { ('+'|'-') mul }
 *   mul     → unary { ('*'|'/'|'%') unary }
 *   unary   → '-' unary | primary
 *   primary → INT_CONST | FLOAT_CONST | ID | '(' expr ')'
 *
 * Children (kid[]) per node kind:
 *   PROGRAM/BLOCK: kid[0] = first statement (rest linked by next)
 *   DECL:   text = type name, kid[0] = first Id (rest linked by next)
 *   ASSIGN: kid[0] = target Id, kid[1] = expr
 *   IF:     kid[0] = cond, kid[1] = then, kid[2] = else (may be NULL)
 *   WHILE:  kid[0] = cond, kid[1] = body
 *   FOR:    kid[0] = init, kid[1] = cond, kid[2] = step, kid[3] = body
 *   BINOP:  text = operator, kid[0] = left, kid[1] = right
 *   NEG:    kid[0] = operand
 *   FUNC:   text = "main", kid[0] = body block
 *   RETURN: kid[0] = expr
 *
 * Error recovery (panic mode): on the first error in a statement we report it,
 * stop reporting, then skip tokens up to the next ';' or '}' and carry on.
 */
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

static int pos;
static int panic;

#define CUR  (tokens[pos])

static void advance(void) { if (CUR.kind != TK_EOF) pos++; }

static void syntax_error(const char *expected)
{
    if (panic) return;
    panic = 1;
    error_at("Syntax", CUR.line, "expected %s but found '%s'", expected, CUR.lexeme);
}

static int accept(TokenKind k)
{
    if (CUR.kind != k) return 0;
    advance();
    return 1;
}

static void expect(TokenKind k, const char *what)
{
    if (!accept(k)) syntax_error(what);
}

static Node *new_node(NodeKind kind, const char *text, int line)
{
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    strncpy(n->text, text, MAX_LEX - 1);
    n->line = line;
    return n;
}

static Node *expr(void);
static Node *stmt(void);

static Node *primary(void)
{
    Token t = CUR;
    if (accept(TK_INUM) || accept(TK_FNUM)) return new_node(N_NUM, t.lexeme, t.line);
    if (accept(TK_ID)) return new_node(N_ID, t.lexeme, t.line);
    if (accept(TK_LPAREN)) {
        Node *e = expr();
        expect(TK_RPAREN, "')'");
        return e;
    }
    syntax_error("an expression");
    return NULL;
}

static Node *unary(void)
{
    Token t = CUR;
    if (accept(TK_MINUS)) {
        Node *n = new_node(N_NEG, "-", t.line);
        n->kid[0] = unary();
        return n;
    }
    return primary();
}

static Node *binop(Token op, Node *l, Node *r)
{
    Node *n = new_node(N_BINOP, op.lexeme, op.line);
    n->kid[0] = l;
    n->kid[1] = r;
    return n;
}

static Node *mul(void)
{
    Node *n = unary();
    while (CUR.kind == TK_STAR || CUR.kind == TK_SLASH || CUR.kind == TK_PERCENT) {
        Token op = CUR;
        advance();
        n = binop(op, n, unary());
    }
    return n;
}

static Node *add(void)
{
    Node *n = mul();
    while (CUR.kind == TK_PLUS || CUR.kind == TK_MINUS) {
        Token op = CUR;
        advance();
        n = binop(op, n, mul());
    }
    return n;
}

static Node *expr(void)
{
    Node *n = add();
    if (CUR.kind >= TK_EQ && CUR.kind <= TK_GE) {
        Token op = CUR;
        advance();
        n = binop(op, n, add());
    }
    return n;
}

static Node *assign(void)
{
    Token t = CUR;
    Node *n = new_node(N_ASSIGN, "=", t.line);
    expect(TK_ID, "a variable name");
    n->kid[0] = new_node(N_ID, t.lexeme, t.line);
    expect(TK_ASSIGN, "'='");
    n->kid[1] = expr();
    return n;
}

static Node *decl(void)
{
    Node *n = new_node(N_DECL, CUR.lexeme, CUR.line);
    advance();   /* 'int' or 'float' */
    Node **tail = &n->kid[0];
    do {
        Token t = CUR;
        expect(TK_ID, "a variable name");
        *tail = new_node(N_ID, t.lexeme, t.line);
        tail = &(*tail)->next;
    } while (accept(TK_COMMA));
    expect(TK_SEMI, "';'");
    return n;
}

/* Parses statements until 'end' (EOF or '}'), recovering from errors. */
static Node *stmt_list(TokenKind end)
{
    Node *first = NULL, **tail = &first;
    while (CUR.kind != end && CUR.kind != TK_EOF) {
        int start = pos;
        Node *s = stmt();
        if (panic) {   /* skip to a safe point, unless the statement already ended at one */
            TokenKind last = pos > start ? tokens[pos - 1].kind : TK_EOF;
            if (last != TK_SEMI && last != TK_RBRACE) {
                while (CUR.kind != TK_SEMI && CUR.kind != TK_RBRACE && CUR.kind != TK_EOF) advance();
                accept(TK_SEMI);
            }
            panic = 0;
        }
        if (pos == start) advance();   /* always make progress */
        if (s) { *tail = s; tail = &s->next; }
    }
    return first;
}

static Node *stmt(void)
{
    Token t = CUR;
    Node *n;
    switch (t.kind) {
    case TK_INT: case TK_FLOAT:
        return decl();
    case TK_ID:
        n = assign();
        expect(TK_SEMI, "';'");
        return n;
    case TK_LBRACE:
        advance();
        n = new_node(N_BLOCK, "{ }", t.line);
        n->kid[0] = stmt_list(TK_RBRACE);
        expect(TK_RBRACE, "'}'");
        return n;
    case TK_IF:
        advance();
        n = new_node(N_IF, "if", t.line);
        expect(TK_LPAREN, "'('");
        n->kid[0] = expr();
        expect(TK_RPAREN, "')'");
        n->kid[1] = stmt();
        if (accept(TK_ELSE)) n->kid[2] = stmt();
        return n;
    case TK_WHILE:
        advance();
        n = new_node(N_WHILE, "while", t.line);
        expect(TK_LPAREN, "'('");
        n->kid[0] = expr();
        expect(TK_RPAREN, "')'");
        n->kid[1] = stmt();
        return n;
    case TK_FOR:
        advance();
        n = new_node(N_FOR, "for", t.line);
        expect(TK_LPAREN, "'('");
        n->kid[0] = assign();
        expect(TK_SEMI, "';'");
        n->kid[1] = expr();
        expect(TK_SEMI, "';'");
        n->kid[2] = assign();
        expect(TK_RPAREN, "')'");
        n->kid[3] = stmt();
        return n;
    case TK_RETURN:
        advance();
        n = new_node(N_RETURN, "return", t.line);
        n->kid[0] = expr();
        expect(TK_SEMI, "';'");
        return n;
    default:
        syntax_error("a statement");
        return NULL;
    }
}

Node *parse(void)
{
    pos = 0;
    panic = 0;
    Node *prog = new_node(N_PROGRAM, "program", 1);

    /* int main() { ... } is optional: without it the whole file is the program */
    if (CUR.kind == TK_INT && tokens[pos + 1].kind == TK_ID && !strcmp(tokens[pos + 1].lexeme, "main")) {
        Node *fn = new_node(N_FUNC, "main", CUR.line);
        pos += 2;
        expect(TK_LPAREN, "'('");
        expect(TK_RPAREN, "')'");
        if (CUR.kind != TK_LBRACE) syntax_error("'{'");
        fn->kid[0] = stmt();   /* the block, parsed with error recovery */
        if (CUR.kind != TK_EOF) {
            panic = 0;
            syntax_error("end of program after main()");
        }
        prog->kid[0] = fn;
        return prog;
    }

    prog->kid[0] = stmt_list(TK_EOF);
    return prog;
}

const char *type_name(Type t)
{
    switch (t) {
    case TY_INT: return "int";
    case TY_FLOAT: return "float";
    case TY_ERROR: return "error";
    default: return "";
    }
}

const char *node_label(Node *n, char *buf)
{
    static const char *names[] = {
        "Program", "Block", "Declaration", "Assign", "If", "While", "For",
        "BinOp", "Negate", "Num", "Id", "Function", "Return"
    };
    switch (n->kind) {
    case N_DECL: case N_BINOP: case N_NUM: case N_ID: case N_FUNC:
        sprintf(buf, "%s %s", names[n->kind], n->text);
        break;
    default:
        strcpy(buf, names[n->kind]);
    }
    return buf;
}
