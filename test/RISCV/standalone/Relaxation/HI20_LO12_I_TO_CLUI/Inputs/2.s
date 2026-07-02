.option relax

.data
.global var
var:
.word 10

.section foo_sec, "ax", @progbits
.global foo
# `foo` and `.data` will be placed in the linker script such that `%hi(var)` is initially
# non-zero, so we relax to `c.lui`. But these relaxations change where `.data` is placed
# in the end, resulting in a `c.lui` with imm 0, which isn't allowed. Check that this
# case is handled in some manner.
foo:
  lui a0, %hi(var)
  lw a0, %lo(var)(a0)
  lui a0, %hi(var)
  lw a0, %lo(var)(a0)
  ret
