## An undefined weak function resolves to 0 and is called through a PLT32
## relocation. The call at 0x80001000 gives a displacement of -0x80001005,
## which is below the signed 32-bit range, so it must be reported.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/call.s -o %t/call.o
# RUN: not %link %linkopts -T %t/layout.lds %t/call.o -o %t/call.out 2>&1 | %filecheck %s

# CHECK: relocation R_X86_64_PLT32 out of range: -2147487749 is not in [-2147483648, 2147483647]

#--- layout.lds
SECTIONS { . = 0x80001000; .text : { *(.text*) } }

#--- call.s
.text
.weak ext
.globl _start
_start:
  call ext@PLT
