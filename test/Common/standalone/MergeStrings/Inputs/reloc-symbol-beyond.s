.section .rodata.str1.1,"aMS",%progbits,1
.Lstr:
  .asciz "foo"

.globl out_of_bounds
.set out_of_bounds, .Lstr + 40

.data
.globl ptr
ptr:
  .long out_of_bounds
