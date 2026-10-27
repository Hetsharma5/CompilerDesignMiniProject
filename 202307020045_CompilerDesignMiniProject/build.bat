@echo off
gcc -std=c99 -Wall -Wextra -o minicc.exe src\main.c src\lexer.c src\parser.c src\semantic.c src\codegen.c src\report.c
if errorlevel 1 exit /b 1
echo Built minicc.exe. Try:  minicc examples\demo.mc
