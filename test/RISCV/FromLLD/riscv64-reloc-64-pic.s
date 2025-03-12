# UNSUPPORTED: riscv32
# RUN: %llvm-mc -filetype=obj -triple=riscv64 %s -o %t.o
# RUN: %link -march riscv64 -shared %t.o -o %t.so
# RUN: llvm-nm %t.so | FileCheck --check-prefix=NM %s
# RUN: llvm-readobj -r %t.so | FileCheck --check-prefix=RELOC %s

## R_RISCV_64 is an absolute relocation type.
## In PIC mode, it creates a relative relocation if the symbol is non-preemptable.

# NM: 00000000000010e8 D b

# RELOC:      .rela.dyn {
# RELOC-NEXT:   0x10E8 R_RISCV_RELATIVE - 0x10E8
# RELOC-NEXT:   0x10E0 R_RISCV_64 a 0
# RELOC-NEXT: }

.globl a, b
.hidden b

.data
.quad a
b:
.quad b
