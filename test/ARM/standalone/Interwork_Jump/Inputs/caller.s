.extern callee
.arm

.type caller, %function
caller:
b callee
b callee_t

.thumb
.type caller_t, %function
caller_t:
b callee
b callee_t
