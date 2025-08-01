
  .option exact

  .global foo

  .text
  .p2align 1
main:
  qc.e.jal callee
  qc.e.beqi a0, 0x100, callee
  qc.e.li a0, callee
  qc.li a0, %qc.abs20(callee)
  ret

  .section .text.other, "ax", %progbits
  .global callee
  .p2align 1
callee:
  ret
  .size callee, .-callee
