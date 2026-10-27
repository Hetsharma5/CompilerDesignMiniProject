// Every phase finds something wrong here (comment out lines to see each one)
int x, y;
float f;

x = 10 @ 2;             // lexical: unknown character
y = (x + 3;             // syntax: missing ')'
if x > 2 { y = 1; }     // syntax: missing '('
y = 2;
