.section .text.foo,"ax",@progbits
.weak foo
jump ##(foo+0x1000)
