//----------SymbolSize.test----------------- Executable------------------#

//BEGIN_COMMENT
// Tests a bug where symbol size was incorrect after relaxation due to
// the linker adjusting the size of the wrong symbol.
//END_COMMENT
//-----------------------------------------------------------------------

.globl foo
.globl bar
.type foo,@function
.type bar,@function
foo:
  ret
bar:
  call foo
.size foo, 4
.size bar, 8

// RUN: %clang %clangopts -c %s -o %t.o
// RUN: %link %linkopts %t.o -o %t.out
// RUN: %readelf -s %t.out 2>&1 | %filecheck %s

// CHECK: {{[0-9]+}} 4 FUNC GLOBAL DEFAULT 1 foo
