  .data
  .p2align 3
  .globl small, big
  .type small, @object
  .type big, @object
  .size small, 1
  .size big, 8
small:
big:
  .quad 0x1122334455667788

  .text
  .globl lib_read_big
  .type lib_read_big, @function
lib_read_big:
  movq big@GOTPCREL(%rip), %rax
  movq (%rax), %rax
  ret
  .size lib_read_big, .-lib_read_big

  .globl lib_addr_big
  .type lib_addr_big, @function
lib_addr_big:
  movq big@GOTPCREL(%rip), %rax
  ret
  .size lib_addr_big, .-lib_addr_big
  .section .note.GNU-stack,"",@progbits
