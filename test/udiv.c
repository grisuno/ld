int printf();

int main(void) {
    unsigned long a = 4000000000ul;
    unsigned long big = a * a;
    printf("%d\n", big > a);
    printf("%d\n", big >= a);
    printf("%d\n", a < big);
    printf("%d\n", a <= big);
    printf("%d\n", a > big);
    printf("%d\n", big < a);
    printf("%d\n", 100 < big);
    printf("%d\n", big > 100);
    printf("%d\n", (big / a) == a);
    printf("%d\n", (big % a) == 0);
    printf("%d\n", big % 7);
    printf("%d\n", big >> 63);
    printf("%d\n", (big >> 62) == 3);
    big /= a;
    printf("%d\n", big == a);
    big = a * a;
    big %= 7;
    printf("%d\n", big);
    {
        unsigned long s = a * a;
        s >>= 63;
        printf("%d\n", s);
    }
    {
        unsigned x = 4000000000u;
        printf("%d\n", x / 2);
        printf("%d\n", x > 100);
    }
    return 0;
}
