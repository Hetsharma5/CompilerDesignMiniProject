// optimizer edge cases
int total, z, k;
float f;
total = 7;              // starts with 't' but is a variable: must stay
z = 7 / 0;              // division by zero: must NOT be folded
k = -(3 - 5) * 2;       // negative numbers fold: k = 4
f = 7 / 2;              // int division, then widened: f = 3.0
f = f * 1.5;            // f = 4.5
while (0) { k = 1; }    // condition always false: jumps straight out
