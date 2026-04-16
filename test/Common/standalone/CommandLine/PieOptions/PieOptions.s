## Check that -pie/--pie and -no-pie/--no-pie are all accepted.

// RUN: %clang %clangopts -fPIC -c %s -o %t.o

// RUN: %link %linkopts %t.o -o %t1.out -pie
// RUN: %link %linkopts %t.o -o %t2.out -no-pie

// RUN: %link %linkopts %t.o -o %t3.out --pie
// RUN: %link %linkopts %t.o -o %t4.out --no-pie

// RUN: %readelf --file-header %t1.out 2>&1 | %filecheck %s --check-prefix=PIE
// RUN: %readelf --file-header %t2.out 2>&1 | %filecheck %s --check-prefix=NOPIE
// RUN: %readelf --file-header %t3.out 2>&1 | %filecheck %s --check-prefix=PIE
// RUN: %readelf --file-header %t4.out 2>&1 | %filecheck %s --check-prefix=NOPIE

// PIE: DYN (Shared object file)
// NOPIE: EXEC (Executable file)

.global foo
foo:
  nop
