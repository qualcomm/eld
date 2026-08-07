.syntax unified
.arm

@ The TLS symbol that the R_ARM_TLS_DESC relocation will reference.
.section .tbss,"awT",%nobits
.globl tls_var
.type tls_var, %object
tls_var:
    .word 0

@ tls_desc_slot holds a R_ARM_TLS_DESC relocation to 'tls_var', forced
@ explicitly via .reloc.
.data
.globl tls_desc_slot
.type tls_desc_slot, %object
tls_desc_slot:
    .reloc tls_desc_slot, R_ARM_TLS_DESC, tls_var
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
