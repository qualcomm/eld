.extern callee32
.extern callee16
.arm
.type caller32, %function
caller32:
push {r4-r5,lr}
bl  callee32
blx callee32
blx callee16
bl  callee16

.thumb
.type caller16, %function
caller16:
push {r4-r5,lr}
bl  callee32
blx callee32
blx callee16
bl  callee16
pop {r4-r5,pc}
