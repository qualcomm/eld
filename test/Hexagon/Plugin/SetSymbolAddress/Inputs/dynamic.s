.text
.globl entry
.hidden entry
.type entry,@function
entry:
{ r0 = memw(r1 + ##data@GOT) }
{ jumpr r31 }

.globl data
.ifndef PREEMPTIBLE
.protected data
.endif
.type data,@object
.ifdef ABSOLUTE
.set data, 0x12345678
.else
.section .dedup,"a",@progbits
data:
.asciz "format"
.size data, .-data
.endif

.data
.globl pointers
.hidden pointers
.type pointers,@object
pointers:
.word data
.word data + 5
.word data - 1
.size pointers, .-pointers

.ifndef ONLY_ABSOLUTE
.text
.globl ordinary_got
.hidden ordinary_got
.type ordinary_got,@function
ordinary_got:
{ r0 = memw(r1 + ##external@GOT) }
{ jumpr r31 }

.data
.globl external
.type external,@object
external:
.word 456
.size external, .-external

.local normal
.type normal,@object
normal:
.word 123
.size normal, .-normal

.globl normal_pointer
.hidden normal_pointer
.type normal_pointer,@object
normal_pointer:
.word normal
.size normal_pointer, .-normal_pointer

.globl external_pointer
.hidden external_pointer
.type external_pointer,@object
external_pointer:
.word external + 7
.size external_pointer, .-external_pointer
.endif
