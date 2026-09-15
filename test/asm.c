int printf();

int probe = 0;

int main(void) {
    __asm__ volatile("nop");
    __asm("nop");
    volatile int v = 5;
    v = v + 1;
    printf("%d %d\n", probe, v);
    __asm__ volatile("movq $777, probe(%rip)");
    printf("%d\n", probe);
    __asm__ volatile("incq probe(%rip)");
    printf("%d\n", probe);
    return 0;
}
