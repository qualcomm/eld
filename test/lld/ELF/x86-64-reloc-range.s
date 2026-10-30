# REQUIRES: x86

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/input.s -o %t/input.o
# RUN: not %link %linkopts -shared -T %t/layout.lds %t/input.o -o %t/out.so 2>&1 | %filecheck %s --check-prefix=OVERFLOW

# OVERFLOW: relocation R_X86_64_PC32 out of range: 2147483648 is not in [-2147483648, 2147483647]

#--- layout.lds
SECTIONS {
  .text 0x1000 : { *(.text*) }
  .bss 0x80001007 : { *(.bss*) }
}

#--- input.s
.text
leaq faraway(%rip), %rax

.bss
.hidden faraway
.globl faraway
faraway:
.zero 1
