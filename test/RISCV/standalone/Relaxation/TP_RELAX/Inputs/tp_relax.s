.option relax

.text
.global bounds_checks
bounds_checks:
## Offset = 8
lui a0, %tprel_hi(sym_in)
add a0, a0, tp, %tprel_add(sym_in)
lw a0, %tprel_lo(sym_in)(a0)

## Offset = 2047
lui a1, %tprel_hi(sym_in_2047)
add a1, a1, tp, %tprel_add(sym_in_2047)
lw a1, %tprel_lo(sym_in_2047)(a1)

## Offset = 2047 using addend
lui a1, %tprel_hi(sym_in + 2039)
add a1, a1, tp, %tprel_add(sym_in + 2039)
lw a1, %tprel_lo(sym_in + 2039)(a1)

# Offset = 2048
lui a2, %tprel_hi(sym_out_2048)
add a2, a2, tp, %tprel_add(sym_out_2048)
lw a2, %tprel_lo(sym_out_2048)(a2)

# Offset = 2048 using addend
lui a2, %tprel_hi(sym_in + 2040)
add a2, a2, tp, %tprel_add(sym_in + 2040)
lw a2, %tprel_lo(sym_in + 2040)(a2)

# Offset = 4096
lui a3, %tprel_hi(sym_out)
add a3, a3, tp, %tprel_add(sym_out)
lw a3, %tprel_lo(sym_out)(a3)

# Test independent LO12_I/LO12_S relocations sharing the hi/add parts. Also
# check extra unrelated instructions don't impact the relaxation.
.global shared_nonsequential_relax
shared_nonsequential_relax:
lui a0, %tprel_hi(sym_in)
add a1, a0, tp, %tprel_add(sym_in)
nop
nop
lw a2, %tprel_lo(sym_in)(a1)
addi a3, a2, 1
sw a3, %tprel_lo(sym_in)(a1)

.section .tbss
.space 8
.globl sym_in
sym_in:
.zero 4
.space 2035
.globl sym_in_2047
sym_in_2047:
.zero 1
.globl sym_out_2048
sym_out_2048:
.zero 1
.space 2047
.globl sym_out
sym_out:
.zero 4
