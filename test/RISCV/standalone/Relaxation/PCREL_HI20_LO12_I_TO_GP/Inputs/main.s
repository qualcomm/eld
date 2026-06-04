## Two HI20/LO12_I pairs that relax to single GP-relative loads.
.globl main
main:
  lui a0, %hi(d)
  lw  a0, %lo(d)(a0)
  lui a1, %hi(d+4)
  lw  a1, %lo(d+4)(a1)
.size main, .-main
.data
d: .zero 8
