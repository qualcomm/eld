# A zero-sized STT_OBJECT alias still identifies the COPY storage.
.data
.p2align 2
.globl payload
.type payload,@object
.globl empty_alias
.type empty_alias,@object
payload:
empty_alias:
.long 0x21451234
.size payload,4
.size empty_alias,0
.text
.globl dso_addr_empty
.type dso_addr_empty,@function
dso_addr_empty:
  movq empty_alias@GOTPCREL(%rip), %rax
  ret
.size dso_addr_empty,.-dso_addr_empty
.section .note.GNU-stack,"",@progbits
