## R_X86_64_PC32 has a signed 32-bit field. The leaq at 0x1000 has its
## displacement field at 0x1003 and an addend of -4. With the target at
## 0x80001006 the value is 0x7fffffff and links. At 0x80001007 it is 0x80000000,
## one past the field, and must be reported.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/lea.s -o %t/lea.o
# RUN: %link %linkopts -T %t/fit.lds %t/lea.o -o %t/fit.out
# RUN: %objdump -d %t/fit.out | %filecheck %s --check-prefix=FIT
# RUN: not %link %linkopts -T %t/over.lds %t/lea.o -o %t/over.out 2>&1 | %filecheck %s --check-prefix=OVER

# FIT: leaq 0x7fffffff(%rip)
# OVER: relocation R_X86_64_PC32 out of range: 2147483648 is not in [-2147483648, 2147483647]

#--- fit.lds
faraway = 0x80001006;
SECTIONS { . = 0x1000; .text : { *(.text*) } }

#--- over.lds
faraway = 0x80001007;
SECTIONS { . = 0x1000; .text : { *(.text*) } }

#--- lea.s
.text
.globl _start
_start:
  leaq faraway(%rip), %rax
