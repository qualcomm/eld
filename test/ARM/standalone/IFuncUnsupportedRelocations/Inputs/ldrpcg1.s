// Reference an IFUNC symbol with R_ARM_LDR_PC_G1.
// This is an INVALID use: R_ARM_LDR_PC_G1 does not belong to any recognised
// IFunc relocation category, so eld warns about an invalid relocation.

  .text
  .global ref
ref:
  .reloc ., R_ARM_LDR_PC_G1, foo
  .word 0
