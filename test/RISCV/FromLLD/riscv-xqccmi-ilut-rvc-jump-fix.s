# REQUIRES: riscv32
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
# The C.J has no relocation: its immediate is resolved by the assembler.
# ILUT compacts the 80 instructions between the target and the jump.  The
# linker must decode the C.J immediate using the RISC-V bit layout and then
# re-encode the shortened displacement.
#
# CHECK: <target>:
# CHECK: c.j {{.*}} <target>

.text
.globl _start
_start:
.globl target
target:
  c.nop
.option push
.option norvc
  .rept 80
  lbu a2, 0(a1)
  .endr
.option pop
.option push
.option rvc
  c.j .-322
.option pop
