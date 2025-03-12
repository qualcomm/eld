        .section .text.foo
        .globl  foo
        .type   foo,@function
foo:
  lui a0, %hi(.L.str + 1)
  addi a0, a0, %lo(.L.str+1)
  lw a2, 0(a0)
  lui a0, %hi(.L.str + 1)
  addi a0, a0, %lo(.rodata.str+2)
  lw a2, 0(a0)
  lui a0, %hi(.rodata.str + 1)
  addi a0, a0, %lo(.rodata.str+2)
  lw a2, 0(a0)

        .type   .L.str,@object
        .section        .rodata.str,"aMS",@progbits,1
.L.str:
        .asciz  "constant string literal"
        .size   .L.str, 24
