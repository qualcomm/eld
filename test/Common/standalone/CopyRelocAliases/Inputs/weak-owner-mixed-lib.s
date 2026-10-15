.section .data
.globl strong_small
.weak weak_big

.type strong_small,@object
.type weak_big,@object

strong_small:
weak_big:
.quad 0x0102030405060708

.size strong_small,4
.size weak_big,8

.section .note.GNU-stack,"",@progbits
