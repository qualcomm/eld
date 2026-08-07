.syntax unified
.arm

@ The TLS symbol that the R_ARM_TLS_DTPOFF32 relocation will reference.
.section .tbss,"awT",%nobits
.globl tls_var
.type tls_var, %object
tls_var:
    .word 0

@ tls_dtpoff32_slot holds a R_ARM_TLS_DTPOFF32 relocation to 'tls_var',
@ forced explicitly via .reloc.
.data
.globl tls_dtpoff32_slot
.type tls_dtpoff32_slot, %object
tls_dtpoff32_slot:
    .reloc tls_dtpoff32_slot, R_ARM_TLS_DTPOFF32, tls_var
    .word 0

.text
.globl main
.type main, %function
main:
    mov r0, #0
    bx  lr
