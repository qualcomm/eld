## R_X86_64_PC8 and R_X86_64_PC16 are range checked. A displacement at the
## edge of the field links: 127 for PC8, 32767 for PC16. One past the edge is an
## error, not a truncated value.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/pc8-fit.s -o %t/pc8-fit.o
# RUN: %link %linkopts %t/pc8-fit.o -o %t/pc8-fit.out
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/pc8-over.s -o %t/pc8-over.o
# RUN: not %link %linkopts %t/pc8-over.o -o %t/pc8-over.out 2>&1 | %filecheck %s --check-prefix=PC8
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/pc16-fit.s -o %t/pc16-fit.o
# RUN: %link %linkopts %t/pc16-fit.o -o %t/pc16-fit.out
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/pc16-over.s -o %t/pc16-over.o
# RUN: not %link %linkopts %t/pc16-over.o -o %t/pc16-over.out 2>&1 | %filecheck %s --check-prefix=PC16

# PC8: relocation R_X86_64_PC8 out of range: 128 is not in [-128, 127]
# PC16: relocation R_X86_64_PC16 out of range: 32768 is not in [-32768, 32767]

#--- pc8-fit.s
.text
.globl _start
_start:
  .reloc ., R_X86_64_PC8, fwd
  .byte 0
  .space 126
fwd:
  nop

#--- pc8-over.s
.text
.globl _start
_start:
  .reloc ., R_X86_64_PC8, fwd
  .byte 0
  .space 127
fwd:
  nop

#--- pc16-fit.s
.text
.globl _start
_start:
  .reloc ., R_X86_64_PC16, fwd
  .short 0
  .space 32765
fwd:
  nop

#--- pc16-over.s
.text
.globl _start
_start:
  .reloc ., R_X86_64_PC16, fwd
  .short 0
  .space 32766
fwd:
  nop
