.section .orphan.align.cst4,"aM",%progbits,4
.p2align 4
.globl orphan_align_c
.type orphan_align_c, %object
.size orphan_align_c, 4
orphan_align_c:
.4byte 0x33333333
.globl orphan_align_incompatible
.type orphan_align_incompatible, %object
.size orphan_align_incompatible, 4
orphan_align_incompatible:
.4byte 0x11111111
.globl orphan_align_compatible_duplicate
.type orphan_align_compatible_duplicate, %object
.size orphan_align_compatible_duplicate, 4
orphan_align_compatible_duplicate:
.4byte 0x22222222
