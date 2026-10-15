.data
.balign 16

.globl size_storage
.hidden size_storage
.type size_storage, STT_OBJECT
size_storage:
  .long 0x12345678
  .long 2
  .long 3
  .long 4
.size size_storage, 16

.globl small_alias
.type small_alias, STT_OBJECT
.set small_alias, size_storage
.size small_alias, 1

.globl full_alias
.type full_alias, STT_OBJECT
.set full_alias, size_storage
.size full_alias, 16
