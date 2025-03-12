.section .text.foo, "ax", @progbits
.p2align 2
nop
.section .text.bar, "ax", @progbits
.p2align 20
nop
.section .text.foo, "ax", @progbits
nop
.section .ignoreme.1, "", @note
nop
.section .ignoreme.2, "", @progbits
nop
