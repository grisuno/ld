int strlen(const char *s);
int memcmp(const void *a, const void *b, int n);

int main(void) {
    int n = strlen("hello");
    if (memcmp("abc", "abc", 3) != 0) return 1;
    return n;
}
