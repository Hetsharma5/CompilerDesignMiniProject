// Same language, written the way a real C program looks
int main() {
    int n, i, fact;
    n = 5;
    fact = 1;
    for (i = 1; i <= n; i = i + 1) {
        fact = fact * i;
    }
    return 0;
}
