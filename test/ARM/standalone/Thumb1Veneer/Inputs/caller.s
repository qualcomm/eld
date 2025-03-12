.extern callee_t

.section .text.caller_t,"ax"

.thumb
.type caller_t, %function
caller_t:
b callee_t
