/* lexer.c - Phase 1: Lexical analysis.
 * Turns the source text into a list of tokens. Skips whitespace and comments
 * (// and slash-star), reports unknown characters and malformed numbers.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

Token *tokens;
int ntokens;
static int cap;

static const struct { const char *word; TokenKind kind; } keywords[] = {
    {"int", TK_INT}, {"float", TK_FLOAT}, {"if", TK_IF},
    {"else", TK_ELSE}, {"while", TK_WHILE}, {"for", TK_FOR},
    {"return", TK_RETURN},
};

static void add(TokenKind kind, const char *start, int len, int line)
{
    if (ntokens == cap) {
        cap = cap ? cap * 2 : 64;
        tokens = realloc(tokens, cap * sizeof(Token));
    }
    Token *t = &tokens[ntokens++];
    if (len >= MAX_LEX) len = MAX_LEX - 1;
    t->kind = kind;
    memcpy(t->lexeme, start, len);
    t->lexeme[len] = '\0';
    t->line = line;
}

void lex(const char *src)
{
    const char *p = src;
    int line = 1;

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* comments */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            int start_line = line;
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line++;
                p++;
            }
            if (!*p) { error_at("Lexical", start_line, "unterminated comment"); break; }
            p += 2;
            continue;
        }

        const char *start = p;

        /* identifiers and keywords */
        if (isalpha((unsigned char)*p) || *p == '_') {
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            int len = p - start;
            TokenKind kind = TK_ID;
            for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; i++)
                if ((int)strlen(keywords[i].word) == len && !strncmp(start, keywords[i].word, len))
                    kind = keywords[i].kind;
            add(kind, start, len, line);
            continue;
        }

        /* numbers: 123 or 12.5 */
        if (isdigit((unsigned char)*p)) {
            TokenKind kind = TK_INUM;
            while (isdigit((unsigned char)*p)) p++;
            if (*p == '.' && isdigit((unsigned char)p[1])) {
                kind = TK_FNUM;
                p++;
                while (isdigit((unsigned char)*p)) p++;
            }
            if (isalpha((unsigned char)*p) || *p == '_' || *p == '.') {
                /* e.g. 9abc or 1.2.3 - swallow the whole thing and report it */
                while (isalnum((unsigned char)*p) || *p == '_' || *p == '.') p++;
                error_at("Lexical", line, "malformed number '%.*s'", (int)(p - start), start);
                continue;
            }
            add(kind, start, p - start, line);
            continue;
        }

        /* two-character operators first */
        TokenKind kind;
        int len = 2;
        if      (p[0] == '=' && p[1] == '=') kind = TK_EQ;
        else if (p[0] == '!' && p[1] == '=') kind = TK_NE;
        else if (p[0] == '<' && p[1] == '=') kind = TK_LE;
        else if (p[0] == '>' && p[1] == '=') kind = TK_GE;
        else {
            len = 1;
            switch (*p) {
            case '+': kind = TK_PLUS; break;
            case '-': kind = TK_MINUS; break;
            case '*': kind = TK_STAR; break;
            case '/': kind = TK_SLASH; break;
            case '%': kind = TK_PERCENT; break;
            case '=': kind = TK_ASSIGN; break;
            case '<': kind = TK_LT; break;
            case '>': kind = TK_GT; break;
            case '(': kind = TK_LPAREN; break;
            case ')': kind = TK_RPAREN; break;
            case '{': kind = TK_LBRACE; break;
            case '}': kind = TK_RBRACE; break;
            case ';': kind = TK_SEMI; break;
            case ',': kind = TK_COMMA; break;
            default:
                error_at("Lexical", line, "unknown character '%c'", *p);
                p++;
                continue;
            }
        }
        add(kind, p, len, line);
        p += len;
    }
    add(TK_EOF, "EOF", 3, line);
}

const char *token_category(TokenKind k)
{
    if (k <= TK_RETURN) return "KEYWORD";
    if (k == TK_ID) return "IDENTIFIER";
    if (k == TK_INUM) return "INT_CONST";
    if (k == TK_FNUM) return "FLOAT_CONST";
    if (k <= TK_GE) return "OPERATOR";
    if (k <= TK_COMMA) return "SEPARATOR";
    return "END";
}
