  # Required until https://github.com/llvm/llvm-project/pull/146184 lands
  .option exact

  # Enable Relaxations
  .option relax

  .text
  .p2align 1
  .globl main
  .type main, @function
main:

  qc.li a0, %qc.abs20(can_c_lui)
  qc.li a0, %qc.abs20(cannot_c_lui)
  qc.li a0, %qc.abs20(can_qc_li)

  qc.li x2, %qc.abs20(can_c_lui) # Cannot due to rd=x2

  qc.e.li a0, can_c_lui
  qc.e.li a0, cannot_c_lui
  qc.e.li a0, can_qc_li
  qc.e.li a0, cannot_qc_li
  qc.e.li a0, can_addi_gprel
  qc.e.li a0, cannot_addi_gprel


  .size main, .-main
