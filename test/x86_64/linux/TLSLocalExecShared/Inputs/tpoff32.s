.section .tdata,"awT",@progbits
.align 4
.globl tls_var32
tls_var32:
  .long 0

.text
.globl get_tls32
get_tls32:
  movl tls_var32@TPOFF, %eax
  ret
