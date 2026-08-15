    .section .text
    .globl main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $8, %rsp
    movl $-16, -8(%rbp)
    movslq -8(%rbp), %rax
    movslq %eax, %rax
    leave
    ret
    .weak _start
    .globl _start
_start:
    call main
    movq %rax, %rdi
    movq $60, %rax
    syscall
