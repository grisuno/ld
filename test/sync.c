int printf();

long ctr = 0;
long flag = 0;

int main(void) {
    long a = 0;
    long b = 0;
    a = __sync_fetch_and_add(&ctr, 5);
    b = __sync_fetch_and_add(&ctr, 3);
    printf("%d %d %d\n", a, b, ctr);
    long old = __sync_lock_test_and_set(&flag, 1);
    printf("%d %d\n", old, flag);
    __sync_lock_release(&flag);
    printf("%d\n", flag);
    __sync_synchronize();
    printf("fence\n");
    return 0;
}
