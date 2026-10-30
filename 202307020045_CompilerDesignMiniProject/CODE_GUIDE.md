# Code Guide: what every file does

This guide goes through the code one file at a time. For the language, the grammar and how to
build and run, see [README.md](README.md).

## The big picture

```
source text
   │  lexer.c       lex()            → tokens[]
   │  parser.c      parse()          → AST (tree of Node)
   │  semantic.c    analyze()        → symtab[] + node->type / node->sym filled in
   │  codegen.c     gen_program()    → code[]  (quadruples / three-address code)
   │  codegen.c     optimize()       → opt[]   (optimized quadruples)
   ▼
main.c  print_all()     → terminal output
report.c write_report() → report.html
```

The phases don't call each other. Each one fills a **global array** declared in `compiler.h`,
and the next phase reads it. Every phase reports problems through one shared function,
`error_at(phase, line, message)`.

`main.c` runs the phases in order and **stops early when there are errors**:
semantic analysis runs only if lexing and parsing found no errors, and code generation and
optimization run only if there are no errors at all.

---

## `src/compiler.h`: shared data structures

Every `.c` file includes this header. It is the "contract" between the phases.

| Struct | Made by | Fields |
|---|---|---|
| `Token` | lexer | `kind` (a `TokenKind` such as `TK_INT`, `TK_ID`, `TK_PLUS`), `lexeme` (the text), `line` |
| `Node` | parser | `kind` (a `NodeKind` such as `N_IF`, `N_BINOP`), `text`, `line`, `kid[4]` (children), `next` (next item in a list), `type` and `sym` (filled in by semantic) |
| `Symbol` | semantic | `name`, `tac` (the name used in TAC), `type`, `size`, `offset`, `scope`, `line` |
| `Quad` | codegen | `op`, `arg1`, `arg2`, `res`: one three-address instruction |
| `Error` | every phase | `phase` ("Lexical", "Syntax", "Semantic"), `line`, `msg` |

Global arrays: `tokens/ntokens`, `symtab/nsyms`, `code/ncode`, `opt/nopt`, `errors/nerrors`.

`MAX_LEX` (64) is the longest name or number. `MAX_ADDR` is a little bigger because a TAC name can
have a `.N` suffix added (see semantic.c).

---

## `src/main.c`: the driver

- **`error_at()`**: works like `printf`. It stores an error in `errors[]` (100 at most). All phases use it.
- **`read_all()`**: reads the whole input file, or the keyboard (stdin), into one string.
- **`print_all()`**: prints every phase in the terminal: the token table, the AST, the symbol table,
  the quadruples before and after optimization, and the errors.
- **`print_tree()`**: draws the AST with `|--` and `` `-- `` branches. It collects the children by
  following both `kid[i]` and the `->next` chain.
- **`main()`**: read source → `lex` → `parse` → `analyze` (if no errors) → `gen_program` + `optimize`
  (if still no errors) → print → write `report.html`. It exits with code 1 if the program had errors.

---

## `src/lexer.c`: Phase 1, lexical analysis

Turns the source text into the `tokens[]` array. `tokens[]` grows with `realloc` as needed.

How `lex()` works, one character at a time:

1. **Newlines** add one to the `line` counter. Other **whitespace** is skipped.
2. **Comments**: `//` skips to the end of the line. `/* ... */` skips to `*/`, still counting lines.
   If the file ends first, it reports *"unterminated comment"*.
3. **Identifiers and keywords**: a letter or `_`, then letters, digits or `_`. The word is checked
   against the `keywords[]` table (`int float if else while for return`). If it isn't a keyword, it's `TK_ID`.
4. **Numbers**: digits give `TK_INUM`, and digits `.` digits give `TK_FNUM`. If a letter, `_` or a
   second `.` follows (`9abc`, `1.2.3`), the whole thing is skipped and reported as a *malformed number*.
5. **Operators**: two-character ones (`== != <= >=`) are checked first, then single characters.
   Anything else is reported as an *unknown character* and skipped.
6. At the end, an `EOF` token is added so the parser always knows where the input stops.

**`token_category()`** turns a token kind into the label shown in the table (KEYWORD, IDENTIFIER,
INT_CONST, FLOAT_CONST, OPERATOR, SEPARATOR). It works because the `TokenKind` enum is listed in groups.

---

## `src/parser.c`: Phase 2, syntax analysis

A **recursive descent parser**: one C function per grammar rule (the grammar is in the comment at the
top of the file). It builds the AST.

Helpers:
- `CUR` is the current token. `advance()` moves to the next one (it never moves past EOF).
- `accept(k)`: if the current token is `k`, consume it and return 1. Otherwise return 0.
- `expect(k, what)`: like `accept`, but reports *"expected X but found Y"* if the token isn't there.
- `new_node()` creates a `Node` with `calloc`, so all children start as NULL.

Expressions, from lowest to highest precedence:
`expr` (comparison, at most one, so `a < b < c` is not allowed) → `add` (`+ -`) → `mul` (`* / %`)
→ `unary` (`-x`) → `primary` (number, name or `( expr )`).
The `while` loops in `add`/`mul` make the operators **left-associative**: `a - b - c` means `(a - b) - c`.

Statements: `stmt()` looks at the first token to decide which rule to use (`int`/`float` → `decl`,
name → `assign`, `{` → block, `if`, `while`, `for`, `return`).

**What `kid[]` means for each node kind** (also in the comment at the top of the file):

| Node | Children |
|---|---|
| PROGRAM / BLOCK | `kid[0]` = first statement, the rest linked by `next` |
| DECL | `text` = type, `kid[0]` = first Id, the rest linked by `next` |
| ASSIGN | `kid[0]` = target Id, `kid[1]` = expression |
| IF | `kid[0]` = condition, `kid[1]` = then, `kid[2]` = else (may be NULL) |
| WHILE | `kid[0]` = condition, `kid[1]` = body |
| FOR | `kid[0]` = init, `kid[1]` = condition, `kid[2]` = step, `kid[3]` = body |
| BINOP | `text` = operator, `kid[0]`, `kid[1]` |
| NEG / RETURN | `kid[0]` = operand / value |
| FUNC | `text` = "main", `kid[0]` = body block |

**Error recovery (panic mode)** is in `stmt_list()`. When `syntax_error()` reports the first
error in a statement, it sets `panic = 1` so that later errors in the same statement aren't
reported. `stmt_list` then skips tokens up to the next `;` or `}`, clears `panic`, and carries on.
That's how one run can find several syntax errors. `if (pos == start) advance();` makes sure the
parser never gets stuck in a loop.

**`parse()`**: if the file starts with `int main`, the program is parsed as a `N_FUNC` holding
one block. Otherwise the whole file is a list of statements.

Also in this file: `type_name()` ("int", "float") and `node_label()` ("BinOp +", "Id a"), used by
both printers.

---

## `src/semantic.c`: Phase 3, semantic analysis

Walks the AST, builds the **symbol table** and checks meaning and types.

**Scopes**: `scope` counts how deep in `{ }` blocks we are. `active[i]` is 1 while symbol `i`'s
block is still open. When a block ends, its symbols are marked inactive. `lookup()` searches
**from the newest symbol back**, so an inner variable hides (shadows) an outer one with the same name.

**`declare()`** checks for these errors:
- *redeclaration* in the same scope
- names like `t1`, `t2`, which are reserved for compiler temporaries
- more than 256 variables

It then sets size = 4, gives the next offset, and records the scope.
If a variable reuses a name that is already in the table (shadowing), it gets its own TAC name such as
`x.3`, so that codegen and the optimizer treat the two variables separately.

**`check_expr()`** works out and stores the type of each expression:
- a number is `float` if it contains `.`, otherwise `int`
- an Id gets the type of its variable. An unknown name gives *undeclared variable*
- comparisons always give `int` (0 or 1)
- arithmetic gives `float` if either side is float (**widening**)
- `%` on a float is an error
- `TY_ERROR` passes upward without new messages, so one mistake gives only one error

**`check_stmt()`**: an assignment of float → int is a *type mismatch*. Blocks open and close a scope.
`return` is allowed only inside `main` (`in_main`) and must return an int.

---

## `src/codegen.c`: Phases 4 and 5, intermediate code and optimization

### Code generation

- `emit(op, a1, a2, res)` adds one quadruple to `code[]`.
- `new_temp()` makes `t1, t2, ...`. `new_label()` makes `L1, L2, ...`.
- **`gen_expr(n, out)`** generates code for an expression and writes the name that holds the result
  into `out`. A number or variable is used directly. An operator makes a new temporary.
- **`widen()`**: when an int is used with a float, it emits `t = (float) x` (the `itof` op) first.
- **`gen_stmt()`** turns control flow into labels and jumps:

```
if (c) S1 else S2        while (c) S             for (i; c; s) S
    t = c                L1: t = c                   i
    ifFalse t goto L1        ifFalse t goto L2   L1: t = c
    S1                       S                       ifFalse t goto L2
    goto L2                  goto L1                 S ; s
L1: S2                   L2:                         goto L1
L2:                                              L2:
```

Declarations produce no code. Variables are always named by `sym->tac`, never `node->text`.

- **`quad_to_tac()`** turns a quadruple into readable text, for example `t1 = a + b` or `ifFalse t2 goto L1`.

### Optimizer: `optimize()`

Copies `code[]` into `opt[]` and improves it. It works inside **basic blocks**: at every label
it forgets all known constants, because control can arrive from somewhere else.

1. **Constant propagation**: `known[]` remembers `name → constant` after `x = 5`. Later uses of
   `x` are replaced with `5`. Assigning a non-constant to a name removes it from `known[]`.
2. **Constant folding**: `fold()` computes `5 * 2 → 10` (int or float math). It returns 0, which
   means "don't fold", for division or `%` by zero. `itof 3` becomes `3.0`, and `- 4` becomes `-4`.
3. **Branch folding**: `ifFalse <non-zero> goto L` is deleted (it never jumps).
   `ifFalse 0 goto L` becomes `goto L`.
4. **Dead temporary removal**: a final pass deletes `tN = constant` when no instruction uses `tN` any more.

This is safe because each temporary is assigned only once and is never used across a label.
Unreachable code is **not** removed.

---

## `src/report.c`: the HTML report

`write_report()` writes one self-contained `report.html` (the CSS is inside it, so it works offline):

- **Pipeline banner**: five `.stage` boxes marked `ok`, `bad` or `skip`. `count_errors()` counts the
  errors for each phase by its name. Phases that didn't run are shown as skipped.
- **Source** with line numbers. Lines with errors are highlighted (`line_has_error()`).
- **Token table**, the **AST** as a tree you can collapse (`<details>`, with `k<N>` classes for colour by
  node kind), the **symbol table**, **TAC before and after** side by side (`code_table()`), and the **error list**.
- `esc()` escapes `< > & "` so the program's text can't break the page.

> The web page reads the `.stage ok|bad|skip` classes from this banner, in order. Keep those class names stable.

---

## `server.py`: the web playground server

Uses only the Python standard library. `Handler` serves the project folder, and `/` opens `web/index.html`.
`POST /compile`:
1. refuses programs larger than 100 KB (413)
2. saves the code as `input.mc` in a new temp folder
3. runs `minicc.exe input.mc` there, with a 5 second timeout (504 if it takes longer)
4. sends back the `report.html` it wrote (500 if there is none)

Listens on `127.0.0.1:8000`. If a `PORT` environment variable is set (on hosting sites such as Render),
it listens on `0.0.0.0:$PORT` instead.

## `web/index.html`: the playground page

A code editor with line numbers on the left and the report on the right. The JavaScript:
- builds the 5 phase chips, makes Tab insert spaces, and compiles on Ctrl+Enter
- the **sample** menu fetches an example `.mc` file and compiles it
- `compile()` sends the code to `/compile`, lights the chips one by one while it waits (skipped if the
  user prefers reduced motion), then colours each chip from the report's `.stage` classes and shows
  the report in an `<iframe srcdoc>`
- if the request fails, it shows *"Could not compile... Is python server.py still running?"*

---

## Scripts and test files

| File | Purpose |
|---|---|
| `build.bat` | `gcc -std=c99 -Wall -Wextra` on all 6 `.c` files → `minicc.exe` |
| `test.bat` | runs each sample and compares the output with `tests/expected/<name>.txt` using `fc` (golden-output tests) |
| `web.bat` | builds if needed, then starts `server.py` |
| `../Dockerfile` | for hosting: builds the compiler for Linux (still named `minicc.exe`) and runs `server.py` |
| `examples/demo.mc` | a full working program: declarations, if/else, while, for, block scope, constant folding |
| `examples/errors.mc` | lexical and syntax errors (shows panic-mode recovery) |
| `examples/semantic.mc` | undeclared, redeclared, float→int, `%` on float |
| `examples/main.mc` | the `int main() { ... return 0; }` form |
| `tests/optimizer.mc` | propagation, folding, branch folding, divide-by-zero not folded |
| `tests/scope.mc` | shadowed variables get separate TAC names (`x.N`) |
| `tests/return_outside.mc`, `tests/main_errors.mc` | `return` rules and errors in the `main` form |

## Adding something new: a checklist

- A new token: add it to `TokenKind` (in the right group, so `token_category()` still works) and to `lex()`.
- A new statement: add a `NodeKind`, a case in `stmt()`, its name in `node_label()`'s `names[]`,
  and cases in `check_stmt()` and `gen_stmt()`.
- New phase output: add it to **both** `print_all()` (terminal) and `write_report()` (HTML).
- After any change to the output: run `test.bat`, check the new output by hand, then update `tests/expected/`.
