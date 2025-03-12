.global callee_t

.section .text.callee_t,"ax"
.thumb
.type callee_t, %function
callee_t:
push {r4-r5,lr}
pop {r4-r5,pc}
