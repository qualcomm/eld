.section .tdata,"awT",@progbits
.align 8
.globl tls_var64
tls_var64:
  .quad 0

.section .data
.align 8
result64:
  .quad tls_var64@TPOFF
