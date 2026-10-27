# End-to-End Mini Compiler with Compiler Phase Visualization

A compiler for a tiny C-like language, written in C. It shows the output of every
compiler phase in the terminal and in a browser report (`report.html`).

```
source --> [1 Lexer] --> tokens --> [2 Parser] --> AST --> [3 Semantic] --> typed AST + symbol table
       --> [4 Intermediate code] --> quadruples / TAC --> [5 Optimizer] --> optimized TAC
```

## Build and run

Needs only gcc (MinGW on Windows).

```
build.bat                       compile -> minicc.exe
.\minicc examples\demo.mc       run on a program (prints every phase, writes report.html)
.\minicc                        type a program yourself, end with Ctrl+Z then Enter
test.bat                        run all sample programs and compare with expected output
```

Then open `report.html` in a browser for the visual version (pipeline status,
token table, collapsible AST, symbol table, TAC before/after optimization, errors).

### Web playground (best for the demo)

```
web.bat                         builds if needed, starts the server, opens http://localhost:8000
```

Type code on the left and press **Compile** (or Ctrl+Enter), or load a sample.
The five phase lights turn green, red or grey, and the full report appears on the right.
`server.py` (Python standard library only) just saves the code, runs `minicc.exe`
on it, and returns the `report.html` it wrote. All the compiling is done in C.

## The language

```c
int a, b;            // declarations: int or float
float avg;
a = 5;               // assignment
b = a * 2 + 3;       // + - * / %   and unary -
if (b > 10) { a = 1; } else { a = 2; }      // < > <= >= == !=
while (a < 10) { a = a + 1; }
for (a = 0; a < 3; a = a + 1) { int sq; sq = a * a; }   // { } opens a new scope
// line comments and /* block comments */
```

The program can also be wrapped in `int main() { ... return 0; }` (see `examples/main.mc`).
`return` is only allowed inside `main`, and must return an int.

Grammar (each rule is one function in `src/parser.c`):

```
program → 'int' 'main' '(' ')' block EOF  |  stmt* EOF
stmt    → decl | assign ';' | if | while | for | block | return
return  → 'return' expr ';'
decl    → ('int' | 'float') ID (',' ID)* ';'
assign  → ID '=' expr
if      → 'if' '(' expr ')' stmt [ 'else' stmt ]
while   → 'while' '(' expr ')' stmt
for     → 'for' '(' assign ';' expr ';' assign ')' stmt
block   → '{' stmt* '}'
expr    → add [ relop add ]
add     → mul { ('+' | '-') mul }
mul     → unary { ('*' | '/' | '%') unary }
unary   → '-' unary | primary
primary → INT_CONST | FLOAT_CONST | ID | '(' expr ')'
```

## Team split (one phase per person)

| Person | Phase | File | Topics it covers |
|---|---|---|---|
| 1 | Lexical analysis | `src/lexer.c` | tokens, keywords, comments, lexical errors |
| 2 | Syntax analysis | `src/parser.c` | recursive descent, AST, syntax error recovery (panic mode) |
| 3 | Semantic analysis | `src/semantic.c` | symbol table, scopes, type checking, undeclared/redeclared variables |
| 4 | Code generation + optimization | `src/codegen.c` | three-address code, quadruples, if/while/for translation, constant folding & propagation |
| shared | Driver + visualization | `src/main.c`, `src/report.c`, `src/compiler.h` | terminal output, HTML report, shared data structures |

## What each phase catches (try `examples/errors.mc` and `examples/semantic.mc`)

- **Lexical:** unknown characters (`@`), malformed numbers (`9abc`, `1.2.3`), unterminated comments
- **Syntax:** missing `;` `(` `)` `}` and similar. The parser reports the error, skips to the next `;` or `}`, and continues, so one run finds several errors.
- **Semantic:** undeclared variable, redeclaration in the same scope, storing a float in an int, `%` on floats

Phases 3–5 only run when the earlier phases found no errors.

## Optimizations (phase 5)

Applied within each basic block (the code between labels):

- **Constant propagation:** `a = 5; t1 = a * 2` becomes `t1 = 5 * 2`
- **Constant folding:** `t1 = 5 * 2` becomes `t1 = 10`. Division by zero is never folded.
- **Branch folding:** `ifFalse 1 goto L1` is removed, and `ifFalse 0 goto L1` becomes `goto L1`
- **Dead temporary removal:** `t1 = 10` is deleted once nothing uses `t1`

Code that becomes unreachable is not removed.
