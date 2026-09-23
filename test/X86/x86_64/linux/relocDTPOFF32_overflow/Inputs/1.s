.section .tdata, "awT", @progbits
.align 4

.globl u
u:
  .long 0x1

.section .data
.align 8

offset_to_u:
  .quad 0
.reloc offset_to_u, R_X86_64_DTPOFF32, u