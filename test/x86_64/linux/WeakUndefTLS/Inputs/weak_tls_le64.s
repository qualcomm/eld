.weak weak_tls
.section .data
.align 8
result:
  .quad 0xdeadbeefdeadbeef
  .reloc result, R_X86_64_TPOFF64, weak_tls
