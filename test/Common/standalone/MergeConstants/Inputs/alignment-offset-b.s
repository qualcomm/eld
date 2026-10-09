.section .rodata.foo,"aM",%progbits,4
.p2align 3
.globl alignment_offset_b
.type alignment_offset_b, %object
.size alignment_offset_b, 4
alignment_offset_b:
.4byte 0x11111111
