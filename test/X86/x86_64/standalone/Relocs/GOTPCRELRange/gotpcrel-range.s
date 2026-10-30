## GOT-relative displacements (GOTPCREL, GOTPCRELX, REX_GOTPCRELX) must be range
## checked like PC-relative ones. With the GOT 4 GiB away from .text, the
## displacement does not fit in 32 bits and must be reported, not truncated.
## With --relax, the relaxed direct form is checked at its own relaxation point.
## A nearby GOT must still link.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/far.s -o %t/far.o
# RUN: not %link %linkopts -T %t/far.lds %t/far.o -o %t/far.out 2>&1 | %filecheck %s --check-prefix=OVERFLOW
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/relax.s -o %t/relax.o
# RUN: not %link %linkopts --relax -T %t/far.lds %t/relax.o -o %t/relax.out 2>&1 | %filecheck %s --check-prefix=RELAX
# RUN: %llvm-mc -filetype=obj -triple=x86_64 %t/near.s -o %t/near.o
# RUN: %link %linkopts -T %t/near.lds %t/near.o -o %t/near.out
# RUN: %objdump -d %t/near.out | %filecheck %s --check-prefix=NEAR

# OVERFLOW-DAG: R_X86_64_GOTPCREL out of range
# OVERFLOW-DAG: R_X86_64_GOTPCRELX out of range
# OVERFLOW-DAG: R_X86_64_REX_GOTPCRELX out of range

# RELAX: relaxing relocation R_X86_64_GOTPCRELX out of range

# NEAR: movq

#--- far.s
.text
.globl _start
_start:
  .long ext@GOTPCREL
  movl ext@GOTPCREL(%rip), %eax
  movq ext@GOTPCREL(%rip), %rax
  call *ext@GOTPCREL(%rip)
.data
.globl ext
ext:
  .quad 0

#--- far.lds
SECTIONS {
  . = 0x1000;
  .text : { *(.text*) }
  . = 0x100000000;
  .got : { *(.got) }
  .data : { *(.data*) }
}

#--- relax.s
.text
.globl _start
_start:
  movl ext@GOTPCREL(%rip), %eax
  call *ext@GOTPCREL(%rip)
.data
.globl ext
ext:
  .quad 0

#--- near.s
.text
.globl _start
_start:
  .long ext@GOTPCREL
  movl ext@GOTPCREL(%rip), %eax
  movq ext@GOTPCREL(%rip), %rax
  call *ext@GOTPCREL(%rip)
.data
.globl ext
ext:
  .quad 0

#--- near.lds
SECTIONS { . = 0x1000; .text : { *(.text*) } }
