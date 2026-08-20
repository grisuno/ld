int write(int fd, char *buf, int n);
int main(int argc, char **argv) {
    char *p = argv[0];
    char *slash = 0;
    for (char *q = p; *q; q++) if (*q == '/') slash = q;
    if (slash) p = slash + 1;
    if (argc > 0) write(1, p, 9);
    return argc;
}