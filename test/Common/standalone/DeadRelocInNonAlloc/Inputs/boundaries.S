.section .text.live,"ax"
.globl _start
_start:
.long 0

.section .text.dead,"ax"
.globl dead
dead:
.long 0

#ifdef UNDEFINED
.section .metadata,""
.long missing
#else
.section .data,"aw"
.globl alloc_ref
alloc_ref:
.long dead+3
.section .debug_info,""
.long dead+7
#endif
