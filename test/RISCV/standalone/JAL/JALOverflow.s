# UNSUPPORTED: riscv64

## Verify the R_RISCV_JAL relocation accepts its full +- 1MiB range and
## rejects the first offset beyond each bound. We place `.text` at
## 0x800000 to keep the disassembly tidy. We check:
##  - small +-0x800 deltas
##  - the positive/negative limits (+0xffffe, -0x100000)
##  - the next aligned offsets beyond the limits (+0x100000, -0x100002)

# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax %s -o  %t.o
# RUN: %link --section-start=.text=0x800000 %t.o --defsym foo=_start+0x800 -o %t.pos
# RUN: %link --section-start=.text=0x800000 %t.o --defsym foo=_start-0x800 -o %t.neg
# RUN: %link --section-start=.text=0x800000 %t.o --defsym foo=_start+0xffffe -o %t.max
# RUN: %link --section-start=.text=0x800000 %t.o --defsym foo=_start-0x100000 -o %t.min
# RUN: %not %link --section-start=.text=0x800000 %t.o --defsym foo=_start+0x100000 -o /dev/null 2>&1 | %filecheck %s --check-prefix=FAILPOS
# RUN: %not %link --section-start=.text=0x800000 %t.o --defsym foo=_start-0x100002 -o /dev/null 2>&1 | %filecheck %s --check-prefix=FAILNEG

# RUN: %objdump -d %t.pos | %filecheck %s --check-prefix=PASSPOS
# RUN: %objdump -d %t.neg | %filecheck %s --check-prefix=PASSNEG
# RUN: %objdump -d %t.max | %filecheck %s --check-prefix=MAX
# RUN: %objdump -d %t.min | %filecheck %s --check-prefix=MIN

# PASSPOS: jal 0x800800 <_start+0x800>
# PASSNEG: jal 0x7ff800 <foo>
# MAX: jal 0x8ffffe <_start+0xffffe>
# MIN: jal 0x700000 <foo>

## lld error: `ld.lld: error: 1.o:(.text+0x0): relocation R_RISCV_JAL out of range: 1048576 is not in [-1048576, 1048575]; references 'foo'`
## lld error: `ld.lld: error: 1.o:(.text+0x0): relocation R_RISCV_JAL out of range: -1048578 is not in [-1048576, 1048575]; references 'foo'`
# FAILPOS: Error: Relocation overflow when applying relocation `R_RISCV_JAL' for symbol `foo'
# FAILNEG: Error: Relocation overflow when applying relocation `R_RISCV_JAL' for symbol `foo'

  .option exact
  .text
  .globl _start
_start:
  jal ra, foo
