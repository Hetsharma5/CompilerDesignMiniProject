/* report.c - Phase visualization: writes one self-contained HTML page that shows
 * the output of every phase. Open report.html in any browser; works offline.
 */
#include <string.h>
#include "compiler.h"

static FILE *out;

/* print text with HTML special characters escaped */
static void esc(const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '<': fputs("&lt;", out); break;
        case '>': fputs("&gt;", out); break;
        case '&': fputs("&amp;", out); break;
        case '"': fputs("&quot;", out); break;
        default: fputc(*s, out);
        }
    }
}

static int count_errors(const char *phase)
{
    int n = 0;
    for (int i = 0; i < nerrors; i++) n += !strcmp(errors[i].phase, phase);
    return n;
}

static int line_has_error(int line)
{
    for (int i = 0; i < nerrors; i++) if (errors[i].line == line) return 1;
    return 0;
}

static void stage(const char *name, const char *status, const char *detail)
{
    fprintf(out, "<div class=\"stage %s\"><b>%s</b><span>%s</span></div>", status, name, detail);
}

static void tree(Node *n)
{
    char label[128];
    int has_kids = n->kid[0] || n->kid[1] || n->kid[2] || n->kid[3];
    fputs(has_kids ? "<details open><summary>" : "<div class=\"leaf\">", out);
    fprintf(out, "<span class=\"k%d\">", n->kind);
    esc(node_label(n, label));
    fputs("</span>", out);
    if (n->type != TY_NONE) fprintf(out, " <i>%s</i>", type_name(n->type));
    if (!has_kids) { fputs("</div>", out); return; }
    fputs("</summary>", out);
    for (int i = 0; i < 4; i++)
        for (Node *c = n->kid[i]; c; c = c->next) tree(c);
    fputs("</details>", out);
}

static void code_table(Quad *q, int n)
{
    char tac[256];
    fputs("<table><tr><th>#</th><th>Three address code</th><th>op</th><th>arg1</th><th>arg2</th><th>result</th></tr>", out);
    for (int i = 0; i < n; i++) {
        quad_to_tac(&q[i], tac);
        fprintf(out, "<tr><td>%d</td><td class=\"tac\">", i);
        esc(tac);
        fputs("</td><td>", out); esc(q[i].op);
        fputs("</td><td>", out); esc(q[i].arg1);
        fputs("</td><td>", out); esc(q[i].arg2);
        fputs("</td><td>", out); esc(q[i].res);
        fputs("</td></tr>", out);
    }
    fputs("</table>", out);
}

static const char *css =
    ":root{--bg:#f6f7f9;--card:#fff;--ink:#1d2433;--mute:#667085;--line:#e3e6ea;--ok:#12805c;--bad:#c4320a;--skip:#98a2b3;--acc:#3557d4;--errbg:#fdecea}"
    "@media (prefers-color-scheme:dark){:root{--bg:#12151b;--card:#1b2029;--ink:#e6e9ef;--mute:#98a2b3;--line:#2c3340;--ok:#3ccf91;--bad:#ff7a59;--skip:#5d6676;--acc:#8fa8ff;--errbg:#3a1d17}}"
    "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,Segoe UI,sans-serif}"
    "main{max-width:1100px;margin:0 auto;padding:24px 16px 64px}h1{margin:0 0 4px;font-size:26px}h2{margin:0 0 12px;font-size:18px}"
    ".sub{color:var(--mute);margin:0 0 20px}section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:18px;margin:16px 0;overflow-x:auto}"
    ".pipe{display:flex;flex-wrap:wrap;gap:8px;align-items:stretch}.stage{flex:1 1 150px;border:2px solid var(--line);border-radius:8px;padding:10px 12px;display:flex;flex-direction:column}"
    ".stage span{color:var(--mute);font-size:13px}.ok{border-color:var(--ok)}.ok b{color:var(--ok)}.bad{border-color:var(--bad)}.bad b{color:var(--bad)}.skip{border-style:dashed;opacity:.6}"
    "code,pre,table,.tree{font-family:Consolas,ui-monospace,monospace;font-size:13.5px}"
    "table{border-collapse:collapse;width:100%}th,td{text-align:left;padding:5px 10px;border-bottom:1px solid var(--line);white-space:pre}th{color:var(--mute);font-weight:600}"
    ".src td:first-child{color:var(--mute);text-align:right;width:1%}.src tr.err{background:var(--errbg)}"
    ".two{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:16px}"
    ".tree details,.tree .leaf{margin-left:18px;border-left:1px dashed var(--line);padding-left:10px}.tree>details{margin-left:0;border:0;padding:0}"
    ".tree summary{cursor:pointer}.tree i{color:var(--mute);font-style:normal;font-size:12px;border:1px solid var(--line);border-radius:4px;padding:0 4px}"
    ".k0,.k1,.k11{font-weight:700}.k12{color:var(--acc);font-weight:600}.k2{color:#9a5bd6}.k3,.k4,.k5,.k6{color:var(--acc);font-weight:600}.k7,.k8{color:#c27c0e}.k9{color:var(--ok)}"
    ".errs li{margin:4px 0}.tag{display:inline-block;font-size:12px;padding:0 6px;border-radius:4px;background:var(--errbg);color:var(--bad);margin-right:6px}"
    ".none{color:var(--mute)}nav a{color:var(--acc);margin-right:14px;font-size:14px}";

void write_report(const char *path, const char *src, Node *ast)
{
    if (!(out = fopen(path, "w"))) { fprintf(stderr, "cannot write %s\n", path); return; }

    int lexe = count_errors("Lexical"), syne = count_errors("Syntax"), seme = count_errors("Semantic");
    int sem_ran = !lexe && !syne, gen_ran = sem_ran && !seme;
    char d[64];

    fprintf(out, "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Mini Compiler Report</title><style>%s</style></head><body><main>", css);
    fputs("<h1>Mini Compiler &mdash; Phase Report</h1><p class=\"sub\">Output of every compiler phase for the program below.</p>", out);
    fputs("<nav><a href=\"#src\">Source</a><a href=\"#lex\">Tokens</a><a href=\"#ast\">AST</a><a href=\"#sym\">Symbol table</a>"
          "<a href=\"#icg\">Intermediate code</a><a href=\"#err\">Errors</a></nav>", out);

    /* pipeline banner */
    fputs("<section><div class=\"pipe\">", out);
    sprintf(d, "%d tokens, %d errors", ntokens, lexe);
    stage("1. Lexer", lexe ? "bad" : "ok", d);
    sprintf(d, "%d errors", syne);
    stage("2. Parser", syne ? "bad" : "ok", d);
    sprintf(d, "%d symbols, %d errors", nsyms, seme);
    stage("3. Semantic", !sem_ran ? "skip" : seme ? "bad" : "ok", sem_ran ? d : "skipped");
    sprintf(d, "%d quadruples", ncode);
    stage("4. Intermediate code", gen_ran ? "ok" : "skip", gen_ran ? d : "skipped");
    sprintf(d, "%d &rarr; %d instructions", ncode, nopt);
    stage("5. Optimizer", gen_ran ? "ok" : "skip", gen_ran ? d : "skipped");
    fputs("</div></section>", out);

    /* source with line numbers, error lines highlighted */
    fputs("<section id=\"src\"><h2>Source program</h2><table class=\"src\">", out);
    const char *p = src;
    for (int line = 1; *p; line++) {
        const char *eol = strchr(p, '\n');
        int len = eol ? (int)(eol - p) : (int)strlen(p);
        char buf[512];
        if (len >= (int)sizeof buf) len = sizeof buf - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
        fprintf(out, "<tr%s><td>%d</td><td>", line_has_error(line) ? " class=\"err\"" : "", line);
        esc(buf);
        fputs("</td></tr>", out);
        p = eol ? eol + 1 : p + len;
    }
    fputs("</table></section>", out);

    /* phase 1 */
    fputs("<section id=\"lex\"><h2>Phase 1 &middot; Lexical analysis &mdash; tokens</h2>"
          "<table><tr><th>#</th><th>Line</th><th>Category</th><th>Lexeme</th></tr>", out);
    for (int i = 0; i < ntokens; i++) {
        fprintf(out, "<tr><td>%d</td><td>%d</td><td>%s</td><td>", i, tokens[i].line, token_category(tokens[i].kind));
        esc(tokens[i].lexeme);
        fputs("</td></tr>", out);
    }
    fputs("</table></section>", out);

    /* phase 2 */
    fputs("<section id=\"ast\"><h2>Phase 2 &middot; Syntax analysis &mdash; abstract syntax tree</h2>"
          "<p class=\"sub\">Click a node to collapse it. Grey tags show the type found in phase 3.</p><div class=\"tree\">", out);
    tree(ast);
    fputs("</div></section>", out);

    /* phase 3 */
    fputs("<section id=\"sym\"><h2>Phase 3 &middot; Semantic analysis &mdash; symbol table</h2>", out);
    if (!sem_ran) fputs("<p class=\"none\">Skipped: fix the lexical/syntax errors first.</p>", out);
    else {
        fputs("<table><tr><th>Name</th><th>Type</th><th>Size</th><th>Offset</th><th>Scope</th><th>Declared on line</th></tr>", out);
        for (int i = 0; i < nsyms; i++) {
            fputs("<tr><td>", out);
            esc(symtab[i].name);
            fprintf(out, "</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
                    type_name(symtab[i].type), symtab[i].size, symtab[i].offset, symtab[i].scope, symtab[i].line);
        }
        fputs("</table>", out);
    }
    fputs("</section>", out);

    /* phase 4 + 5 */
    fputs("<section id=\"icg\"><h2>Phase 4 &amp; 5 &middot; Intermediate code and optimization</h2>", out);
    if (!gen_ran) fputs("<p class=\"none\">Skipped: the program has errors.</p>", out);
    else {
        fputs("<div class=\"two\"><div><h2>Before optimization</h2>", out);
        code_table(code, ncode);
        fputs("</div><div><h2>After optimization</h2>", out);
        code_table(opt, nopt);
        fputs("</div></div>", out);
    }
    fputs("</section>", out);

    /* errors */
    fputs("<section id=\"err\"><h2>Errors</h2>", out);
    if (!nerrors) fputs("<p class=\"none\">No errors. The program compiled successfully.</p>", out);
    else {
        fputs("<ul class=\"errs\">", out);
        for (int i = 0; i < nerrors; i++) {
            fprintf(out, "<li><span class=\"tag\">%s</span>line %d: ", errors[i].phase, errors[i].line);
            esc(errors[i].msg);
            fputs("</li>", out);
        }
        fputs("</ul>", out);
    }
    fputs("</section></main></body></html>\n", out);
    fclose(out);
}
