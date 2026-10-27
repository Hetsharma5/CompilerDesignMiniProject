int x;
x = 1;
{
    float x;      // shadows the outer x
    x = 2.5;
}
x = x + 1;
