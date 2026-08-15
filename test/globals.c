int printf();
int puts(char *s);

int counter = 5;
char letter = 'K';
int table[4] = {10, 20, 30, 40};
char text[] = "global string";
char *ptr = "pointed string";
int zeroed[3];

int main(void) {
    printf("counter=%d\n", counter);
    printf("letter=%d\n", letter);
    printf("table=%d,%d,%d,%d\n", table[0], table[1], table[2], table[3]);
    puts(text);
    puts(ptr);
    printf("zeroed=%d\n", zeroed[1]);
    return counter;
}
