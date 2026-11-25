//----------SymbolSize.test----------------- Executable------------------#

//BEGIN_COMMENT
// Tests that relaxation updates function sizes and symbol offsets correctly,
// including function sizes derived from assembly instruction ranges.
//END_COMMENT
//-----------------------------------------------------------------------

.globl foo
.globl bar
.type foo,@function
.type bar,@function
foo:
  ret
.size foo, .-foo
bar:
  call foo
  call foo
  call foo
.size bar, .-bar
.globl boundary
.set boundary, bar + 2
.size boundary, 0

// RUN: %clang -target riscv32 %clangopts -c %s -o %t.rv32.o
// RUN: %link %linkopts %t.rv32.o -o %t.rv32.out
// RUN: %readelf -s %t.rv32.out 2>&1 | %filecheck %s --check-prefix=RV32
// RUN: %clang -target riscv64 %clangopts -c %s -o %t.rv64.o
// RUN: %link %linkopts %t.rv64.o -o %t.rv64.out
// RUN: %readelf -s %t.rv64.out 2>&1 | %filecheck %s --check-prefix=RV64

// RV32: {{[0-9]+}} 2 FUNC GLOBAL DEFAULT 1 foo
// RV32: {{[0-9]+}} 6 FUNC GLOBAL DEFAULT 1 bar
// RV32: {{[0-9]+}} 0 FUNC GLOBAL DEFAULT 1 boundary
// RV64: {{[0-9]+}} 2 FUNC GLOBAL DEFAULT 1 foo
// RV64: {{[0-9]+}} 12 FUNC GLOBAL DEFAULT 1 bar
// RV64: {{[0-9]+}} 0 FUNC GLOBAL DEFAULT 1 boundary
