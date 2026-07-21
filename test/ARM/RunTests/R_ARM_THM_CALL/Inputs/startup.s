.syntax unified
.thumb

.global _start
.type _start, %function
_start:
    @ Thumb BL to an external symbol naturally emits R_ARM_THM_CALL.
    bl main
    movs r7, #1
    svc  0
