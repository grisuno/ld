    .section .text
    .globl main
main:
    movq $3, %rax
    ret
    .weak _start
    .globl _start
_start:
    movq $9, %rdi
    movq $60, %rax
    syscall
