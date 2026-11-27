.section .text.live,"ax"
.globl _start
_start:
.long 0

.section .text.discarded,"ax"
.globl dead
dead:
.long 0

#ifdef NONDEBUG
.section .metadata,"",%note
.balign 4
.long 4, 24, 1
.asciz "ELD"
#else
.section .debug_discarded,""
#endif
.long dead
.long dead+7
.long .text.discarded
.long .text.discarded+11
.long _start+3
.long absolute+5

// A matching rule must not process a source section that is itself discarded.
.section .discarded,""
.long dead+13
.long .text.discarded+19
