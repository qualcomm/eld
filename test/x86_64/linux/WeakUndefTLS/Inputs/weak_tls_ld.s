.weak weak_tls
.section .data
.align 4
result:
  .long 0xdeadbeef
  .reloc result, R_X86_64_DTPOFF32, weak_tls
