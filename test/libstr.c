int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

int memcmp(const void *a, const void *b, int n) {
    const unsigned char *x = a;
    const unsigned char *y = b;
    int i = 0;
    while (i < n && x[i] == y[i]) i++;
    if (i == n) return 0;
    return (int)x[i] - (int)y[i];
}
