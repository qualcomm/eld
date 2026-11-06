## A PC-relative displacement that does not fit in 32 bits must be reported,
## not truncated. Before the fix, a far call silently wrapped to a wrong target.
## A backward call with an in-range negative displacement must still link.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/far.s -o %t/far.o
# RUN: not %link %linkopts -T %t/far.lds %t/far.o -o %t/far.out 2>&1 | %filecheck %s --check-prefix=OVERFLOW
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/near.s -o %t/near.o
# RUN: %link %linkopts -T %t/near.lds %t/near.o -o %t/near.out
# RUN: %objdump -d %t/near.out | %filecheck %s --check-prefix=NEAR

# OVERFLOW-DAG: R_X86_64_PLT32{{.*}}out of range
# OVERFLOW-DAG: R_X86_64_PC32{{.*}}out of range

# NEAR: e8 fa ff ff ff

#--- far.s
.text
.globl _start
_start:
  call faraway@PLT
  leaq faraway(%rip), %rax

#--- far.lds
faraway = 0x100000000;
SECTIONS { . = 0x1000; .text : { *(.text*) } }

#--- near.s
.text
.globl helper
helper:
  ret
.globl _start
_start:
  call helper

#--- near.lds
SECTIONS { . = 0x1000; .text : { *(.text*) } }
