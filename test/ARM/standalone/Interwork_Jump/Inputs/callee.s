.global callee
.global callee_t

.arm
.type callee, %function
callee:
push {r4-r5,lr}
pop {r4-r5,pc}

.thumb
.type callee_t, %function
callee_t:
push {r4-r5,lr}
pop {r4-r5,pc}
