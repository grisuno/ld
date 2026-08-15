int write(int fd, char *buf, int n);
int main(int argc, char **argv) {
    if (argc > 0) write(1, argv[0], 11);
    return argc;
}
