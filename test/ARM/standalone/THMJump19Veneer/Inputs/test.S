.arch armv8-m.main
.syntax unified
.thumb

.section .text.caller, "ax", %progbits
.thumb_func
.global my_func
.type   my_func, %function
my_func:
    cmp  r0, #0
    beq  plain_entry
    movs r0, #1
    bx   lr

.section .text.target, "ax", %progbits
.global plain_entry
plain_entry:
    movs r0, #0
    bx   lr
