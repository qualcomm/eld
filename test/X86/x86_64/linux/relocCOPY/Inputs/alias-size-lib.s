.data
.p2align 2

.globl size_base
.hidden size_base
.type size_base, @object
size_base:
    .long 0x12345678
.size size_base, 4

.globl small_alias
.type small_alias, @object
.set small_alias, size_base
.size small_alias, 1

.globl full_alias
.type full_alias, @object
.set full_alias, size_base
.size full_alias, 4

.section .note.GNU-stack,"",@progbits
