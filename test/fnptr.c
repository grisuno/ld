int printf();

int add2(int a, int b) {
    return a + b;
}

int mul2(int a, int b) {
    return a * b;
}

int apply2(int (*f)(int, int), int x, int y) {
    return f(x, y);
}

typedef struct {
    int (*op)(int, int);
    int base;
} ops_t;

int (*gop)(int, int);

int run_op(ops_t *o, int x, int y) {
    return o->op(x, y) + o->base;
}

int main(void) {
    int (*f)(int, int);
    int (*tbl[2])(int, int);
    ops_t o;
    f = add2;
    printf("%d\n", f(10, 20));
    f = mul2;
    printf("%d\n", f(10, 20));
    printf("%d\n", apply2(add2, 5, 6));
    printf("%d\n", apply2(mul2, 5, 6));
    o.op = add2;
    o.base = 100;
    printf("%d\n", run_op(&o, 1, 2));
    o.op = mul2;
    printf("%d\n", run_op(&o, 3, 4));
    tbl[0] = add2;
    tbl[1] = mul2;
    printf("%d\n", tbl[0](7, 8) + tbl[1](7, 8));
    f = 0;
    printf("%d\n", f == 0);
    gop = mul2;
    printf("%d\n", gop(6, 7));
    f = mul2;
    printf("%d\n", (*f)(3, 4));
    return 0;
}
