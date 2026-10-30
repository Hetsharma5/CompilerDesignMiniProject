# Brag plan: Mini Compiler

**What it is:** A compiler for a small C-like language, written in C, that shows the output of all five phases (lexer, parser, semantic, intermediate code, optimizer) in the terminal and in a browser playground.
**Who it's for:** Compiler Design students and examiners who want to see what each phase actually does.
**What sets it apart:** It doesn't just compile. It shows you every step: tokens, AST, symbol table, quadruples, and optimized TAC.
**Hook:** Type `b = a * 2 + 3;` and ask how a machine could ever run that.
**Angle:** A 60-second teaching video. Follow one line, `b = a * 2 + 3;` (taken from `examples/demo.mc`), through every phase and use the compiler's real output at each stage.
**Tone:** polished and educational. Clean cards, a steady beat, and a soft lo-fi synth bed. The user asked for about 60s, which overrides the usual 15 to 25s limit.
**Identity:** The playground's dark palette (`#12151b` bg, `#8fa8ff` accent, `#3ccf91` ok, `#f5b942` run, `#ff7a59` bad, `#9a5bd6` keyword). Fonts are Segoe UI/system-ui with Consolas for code, as in `web/index.html`.
**Persistent device:** The playground's phase chips run along the top. A highlight pill moves from phase to phase so the viewer always knows where they are.

## Storyboard (63s, 30fps, 120 BPM)

| # | Time | Scene | On screen |
|---|---|---|---|
| 1 | 0–5.5 | Hook | `b = a * 2 + 3;` types in. "You write this." / "How does it become something a machine can run?" |
| 2 | 5.5–11 | The pipeline | Five phase cards pop in (in → out for each). A pulse turns them green, then they collapse into the top bar |
| 3 | 11–20 | 01 Lexical analysis | A scanner sweeps the line and each lexeme drops into a token card (IDENTIFIER, OPERATOR, INT_CONST…). Whitespace is thrown away. Errors it catches: `@`, `9abc` |
| 4 | 20–30 | 02 Syntax analysis | The AST grows node by node. Grammar rules `add → mul {+ mul}`, `mul → unary {* unary}`. The `*` subtree glows: "parsed one level deeper → binds tighter" |
| 5 | 30–39 | 03 Semantic analysis | Real symbol table rows (name/type/size/offset/scope). `sq` is scope 1. Type check `avg = (a + b) / 2.0` → `(float)` conversion inserted. Error chips |
| 6 | 39–48 | 04 Intermediate code | The tree flattens bottom-up into `t1 = a * 2`, `t2 = t1 + 3`, `b = t2`, then the quadruple table. `if` → `ifFalse t3 goto L1` |
| 7 | 48–56.5 | 05 Optimizer | Constant propagation → folding → `b = 13`, and dead temps collapse. Counter 35 → 30 instructions (real demo.mc output) |
| 8 | 56.5–63 | Outro | The real `report.html` scrolls in a browser frame and all five chips are green. "Mini Compiler. Five phases. Every one visualized." `web.bat → localhost:8000` |

## Sound
The track is synthesized in A minor at 120 BPM (Am7 → Fmaj7 → Cmaj7 → G6). It opens on a pad and typing ticks, then kick, bass and arpeggio come in at 5.5s. Sound effects are pitched to the chord tones and share the same reverb: token pops, node plucks, soft whooshes on scene changes, and a sparkle on each fold. Everything resolves on Am9 at the outro.

No secrets or personal data. All code and numbers come from `examples/demo.mc` and `tests/expected/demo.txt`.
