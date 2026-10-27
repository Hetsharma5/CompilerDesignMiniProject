// Demo program for the mini compiler
int a, b, c, i;
float avg;

a = 5;
b = a * 2 + 3;          // folds to b = 13

if (b > 10) {
    c = b - a;
} else {
    c = 0;
}

while (a < 10) {
    a = a + 1;
}

for (i = 0; i < 3; i = i + 1) {
    int sq;             // block scope: sq lives only inside the loop
    sq = i * i;
    c = c + sq;
}

avg = (a + b) / 2.0;    // int + float -> float, needs (float) conversion
