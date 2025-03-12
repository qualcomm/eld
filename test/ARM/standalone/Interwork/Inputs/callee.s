.global callee32
.global callee16

.arm
.type callee32, %function
callee32:
  push {r4-r5,lr}
  pop {r4-r5,pc}

.thumb
.type callee16, %function
callee16:
push {r4-r5,lr}
add r0, r1, r1
add r1, r2, r3
pop {r4-r5,pc}

