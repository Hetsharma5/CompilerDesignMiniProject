# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

College compiler-design mini project (team of 4): an end-to-end compiler for a tiny C-like language, written in plain C99, that prints every phase and writes `report.html`. See README.md for the language, grammar and team split.

## Commands (Windows, MinGW gcc 6.3, no make)

- Build: `cmd /c .\build.bat` (gcc -std=c99 -Wall -Wextra; keep it warning-free)
- Run: `.\minicc.exe examples\demo.mc` (exit code 1 if the program has errors)
- Test: `cmd /c .\test.bat`. This is a golden-output test: each `examples/*.mc` and `tests/*.mc` is compared with `tests/expected/<name>.txt`. To add a case, add its path to the `for` list in test.bat. After an intended output change, check the new output by hand, then regenerate with `cmd /c ".\minicc.exe X.mc > tests\expected\X.txt"`.
- From PowerShell, exes in the current directory need the `.\` prefix, including inside the .bat files.

## Architecture

- `src/compiler.h` holds every shared struct (Token, Node, Symbol, Quad, Error) and the global arrays (`tokens`, `symtab`, `code`, `opt`, `errors`). Phases communicate only through these globals.
- `main.c` runs lex → parse → analyze → gen_program → optimize. Semantic analysis runs only when there are no errors so far, and codegen/optimizer only when there are none at all. It also owns the terminal printer and `error_at()`, which every phase uses with a phase name ("Lexical", "Syntax", "Semantic"). `report.c` counts errors per phase by that string.
- AST `kid[]` meaning depends on node kind (documented at the top of parser.c). Lists (statements, declared Ids) are chained through `next`, so tree walkers must follow both `kid[i]` and `->next`.
- The semantic phase writes `node->type` and `node->sym` on Id nodes. Codegen relies on the type to insert `itof` conversions, and must name variables with `sym->tac`, not `node->text`. A shadowing variable gets a unique TAC name such as `x.1`; without it the optimizer would mix up the two variables' values (see `tests/scope.mc`).
- The optimizer assumes temporaries `t<N>` are single-assignment and never live across labels. It clears known constants at every label. Users can't declare `t<digits>` names (enforced in semantic.c, and relied on by `is_temp` in codegen.c).
- Web playground: `web.bat` / `python server.py` serves `web/index.html` on 127.0.0.1:8000. POST `/compile` runs minicc.exe in a temp dir and returns its report.html. The page colours its phase chips by reading the `.stage ok|bad|skip` classes from that report, so keep those class names and their order stable.
- Terminal output and the HTML report are written separately (main.c and report.c). A new phase output needs to be added to both.
