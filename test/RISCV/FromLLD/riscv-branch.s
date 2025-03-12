
# UNSUPPORTED: riscv64
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=-relax -riscv-asm-relax-branches=0 %s -o %t.rv32.o

# RUN: %link %linkopts %t.rv32.o --defsym foo=_start+4 --defsym bar=_start -o %t.rv32
# RUN: llvm-objdump -d %t.rv32 | FileCheck %s --check-prefix=CHECK-32
# CHECK-32: 00000263     beqz    zero, 0x58
# CHECK-32: fe001ee3     bnez    zero, 0x54
#
# RUN: %link %linkopts %t.rv32.o --defsym foo=_start+0xffe --defsym bar=_start+0xffe -o %t.rv32.limits
# RUN: llvm-objdump -d %t.rv32.limits | FileCheck --check-prefix=LIMITS-32 %s
# LIMITS-32:      7e000fe3     beqz    zero, 0x1052
# LIMITS-32-NEXT: 7e001de3     bnez    zero, 0x1052

# RUN: not %link %linkopts %t.rv32.o --defsym foo=_start+0x1000 --defsym bar=_start+4-0x1002 -o /dev/null 2>&1 | FileCheck --check-prefix=ERROR-RANGE %s
# ERROR-RANGE: Relocation overflow when applying relocation `R_RISCV_BRANCH' for symbol `foo'
# ERROR-RANGE: Relocation overflow when applying relocation `R_RISCV_BRANCH' for symbol `bar'

# RUN: %link %linkopts %t.rv32.o --defsym foo=_start+1 --defsym bar=_start-1 -o %t.out 2>&1 | FileCheck --check-prefix=WARN-ALIGN %s
# WARN-ALIGN: Relocation `R_RISCV_BRANCH' for symbol `foo' referred from `(Not Applicable)' and defined in `{{.*}}' has alignment 2 but is not aligned

.global _start
_start:
     beq x0, x0, foo
     bne x0, x0, bar
