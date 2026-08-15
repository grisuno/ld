int printf();

int main(void) {
    printf("plain\n");
    printf("d=%d\n", 42);
    printf("s=%s|\n", "str");
    printf("pct=%%\n");
    printf("w=%5d|\n", 7);
    printf("z=%05d|\n", 7);
    printf("o=%o|\n", 64);
    printf("neg=%d\n", 0 - 9);
    printf("two=%d,%d\n", 1, 2);
    printf("three=%d,%d,%d\n", 3, 4, 5);
    printf("mix=%s/%d/%o\n", "m", 6, 8);
    printf("four=%d,%d,%d,%d\n", 1, 2, 3, 4);
    printf("five=%d,%d,%d,%d,%d\n", 1, 2, 3, 4, 5);
    return 0;
}
