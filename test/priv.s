    .section .text
    .globl main
main:
    nop
    cli
    sti
    hlt
    rdtsc
    movq $0, %rax
    ret
    .weak _start
    .globl _start
_start:
    movq $60, %rax
    syscall
