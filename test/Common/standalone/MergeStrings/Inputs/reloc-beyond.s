.section .rodata.str1.1,"aMS",%progbits,1
  .asciz "foo"

.data
.globl ptr
ptr:
  .long .rodata.str1.1 + 40
