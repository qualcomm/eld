  .text
  .globl _start
_start:
  call get_a
  movq %rax, %rbx
  call get_b
  cmpq %rax, %rbx
  jne .Lfail

  movl $0, %edi
  jmp .Lexit

.Lfail:
  movl $1, %edi

.Lexit:
  movl $60, %eax
  syscall
