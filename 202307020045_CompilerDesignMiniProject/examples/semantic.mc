// Parses fine, but has meaning (semantic) errors
int x, y;
float f;
int x;                  // redeclaration

f = 2.5;
x = f * 2;              // float assigned to int
y = f % 2;              // % needs ints (and float to int)
z = 4;                  // undeclared
{
    int y;              // OK: new scope
    y = x;
}
