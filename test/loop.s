    .section .text
    .globl main
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $0, %rax
    movq $0, %rcx
.Lloop:
    cmpq $100, %rcx
    jge .Ldone
    addq %rcx, %rax
    addq $1, %rcx
    jmp .Lloop
.Ldone:
    leave
    ret
    leave
    ret
    .weak _start
    .globl _start
_start:
    call main
    movq %rax, %rdi
    movq $60, %rax
    syscall
