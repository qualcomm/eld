.section .rodata.foo,"aM",%progbits,4
.p2align 4
.globl alignment_offset_c
.type alignment_offset_c, %object
.size alignment_offset_c, 12
alignment_offset_c:
.4byte 0x33333333
.globl alignment_offset_c_duplicate
.type alignment_offset_c_duplicate, %object
.size alignment_offset_c_duplicate, 4
alignment_offset_c_duplicate:
.4byte 0x11111111
.globl alignment_offset_c_tail
.type alignment_offset_c_tail, %object
.size alignment_offset_c_tail, 4
alignment_offset_c_tail:
.4byte 0x22222222
