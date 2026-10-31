.section .data
.weak weak_a
.type weak_a,@object
weak_a:
.quad 0x0102030405060708
.size weak_a,8

.weak weak_b
.type weak_b,@object
.set weak_b,weak_a
.size weak_b,8

.section .note.GNU-stack,"",@progbits
