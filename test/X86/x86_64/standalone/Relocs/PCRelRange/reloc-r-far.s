## With -r the output is relocatable, so the final address is not known and a
## PC32 displacement is not checked. The target is placed 4 GiB away, and the
## relocation must be kept in the output for a later link.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/lea.s -o %t/lea.o
# RUN: %link %linkopts -r -T %t/far.lds %t/lea.o -o %t/rel.o
# RUN: %objdump -dr %t/rel.o | %filecheck %s

# CHECK: R_X86_64_PC32 target-0x4

#--- far.lds
target = 0x100000000;
SECTIONS { . = 0x1000; .text : { *(.text*) } }

#--- lea.s
.text
.globl _start
_start:
  leaq target(%rip), %rax
