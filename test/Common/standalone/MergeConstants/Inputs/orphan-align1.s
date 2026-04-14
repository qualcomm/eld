.section .orphan.align.cst4,"aM",%progbits,4
.p2align 3
.globl orphan_align1
.type orphan_align1, %object
.size orphan_align1, 4
orphan_align1:
.4byte 0x11111111
.zero 4
.globl orphan_align_compatible
.type orphan_align_compatible, %object
.size orphan_align_compatible, 4
orphan_align_compatible:
.4byte 0x22222222
