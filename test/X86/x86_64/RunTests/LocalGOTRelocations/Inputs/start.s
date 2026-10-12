.text
.globl _start
.type _start, @function
_start:
    call test_local_plain
    cmpq $0x12345678, (%rax)
    jne .Lfail

    call test_local_rex
    cmpq $0x12345678, (%rax)
    jne .Lfail

    xorl %edi, %edi
    jmp .Lexit

.Lfail:
    movl $1, %edi

.Lexit:
    movl $60, %eax
    syscall
.size _start, .-_start
