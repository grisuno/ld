/* Host dlopen harness for the -shared proof (test/run_tests.sh).
 * Opens the freshly linked library, resolves both exports and calls
 * them: any broken dynamic table (missing SONAME, bad hash chain,
 * wrong symbol values) fails here with a dlerror instead of passing
 * silently. Plain C99 + libdl, no MiniOS headers. */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv) {
    void *h;
    int (*add)(int, int);
    int (*len)(const char *);
    int sum;
    int n;
    if (argc < 2) {
        printf("usage: dynlib_host lib.so\n");
        return 2;
    }
    h = dlopen(argv[1], 1);
    if (!h) {
        printf("dlopen fail: %s\n", dlerror());
        return 1;
    }
    add = dlsym(h, "dynadd");
    len = dlsym(h, "dynlen");
    if (!add || !len) {
        printf("dlsym fail: %s\n", dlerror());
        return 1;
    }
    sum = add(40, 2);
    n = len("hello");
    printf("dynlib: %d %d\n", sum, n);
    return sum == 42 && n == 5 ? 0 : 1;
}
