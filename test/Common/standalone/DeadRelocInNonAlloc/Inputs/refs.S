.section .text.live,"ax"
.globl _start
_start:
.long absolute

.section .text.dead,"ax"
.globl dead
dead:
.long 0

.section .debug_loc,""
.long dead
.long dead+7
.long _start+3
.long absolute+5

.section .debug_ranges,""
.long dead
.long dead+7

.section .debug_other,""
.long dead+7

#ifdef NONDEBUG
// A note survives GC without keeping its nonalloc references alive.
.section .metadata,"",%note
.balign 4
.long 4, 12, 1
.asciz "ELD"
.long dead+7
.long _start+3
.long absolute+5
#endif

.section .discarded,""
.long .text.dead
