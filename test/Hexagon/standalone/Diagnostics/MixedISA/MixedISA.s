// Check that mixing v71t objects with other ISAs emits a warning
// and that the warning can be supressed.

nop

// RUN: %llvm-mc -filetype=obj -triple=hexagon -mcpu=hexagonv71t %s -o %t.o
// RUN: %llvm-mc -filetype=obj -triple=hexagon -mcpu=hexagonv73 %s -o %t2.o

// RUN: %link %linkopts %t.o %t2.o -o %t.out 2>&1 | %filecheck %s --check-prefix=WA7173
// RUN: %link %linkopts %t2.o %t.o -o %t2.out 2>&1 | %filecheck %s --check-prefix=WA7371

// RUN: %link %linkopts --no-warn-mismatch %t.o %t2.o -o %t3.out 2>&1 \
// RUN:  | %filecheck --allow-empty %s --check-prefix=NOWARN

// WA7173: Warning: Mixing incompatible object file {{.*}} object file arch is hexagonv73, {{.*}}hexagonv71t
// WA7371: Warning: Mixing incompatible object file {{.*}} object file arch is hexagonv71t, {{.*}} hexagonv73
// NOWARN-NOT: Mixing incompatible object file