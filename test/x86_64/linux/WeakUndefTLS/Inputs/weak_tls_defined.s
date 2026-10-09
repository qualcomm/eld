.section .tdata, "awT", @progbits
.align 4
.global real_tls
real_tls:
  .long 0

.section .data
.align 4
result:
  .long 0xdeadbeef
  .reloc result, R_X86_64_TPOFF32, real_tls
