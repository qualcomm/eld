# REQUIRES: riscv32 || riscv64

## Check that R_RISCV_RELAX does not apply to a wrong relocation.

# RUN: %llvm-mc -filetype=obj -mattr=+c,+relax %s -o %t.o
# RUN: %link %linkopts -T %p/Inputs/riscv-pending-bug1276/script.t %t.o -o %t.out
# RUN: %objdump --no-show-raw-insn -M no-aliases -h -d %t.out | FileCheck %s --check-prefix=CHECK

        .data
num:
        .word   1
val:
        .word   2

# CHECK-LABEL: <.text>
        .text
        .option norelax
## This lui should not get relaxed because of incorrectly ordered R_RISCV_RELAX.
# CHECK-NEXT:    lui a3, 0x1234
        lui     a3, %hi(num)
        .option relax
## GP relaxation is not applied to inverted pcrel_hi/lo pairs, see m_DisableGPRelocs
# CHECK-NEXT:    sw zero, 0x55c(a0)
        sw      zero, %pcrel_lo(.Lpcrel_hi5)(a0)
.Lpcrel_hi5:
# CHECK-LABEL: <.Lpcrel_hi5>
# CHECK-NEXT:    auipc a0, 0x1
        auipc   a0, %pcrel_hi(val)
# CHECK-NEXT:    lw a1, 0x4(gp)
        lw      a1, %pcrel_lo(.Lpcrel_hi5)(a0)
