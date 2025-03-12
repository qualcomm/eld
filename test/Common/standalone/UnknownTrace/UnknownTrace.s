//---UnknownTrace.test--------------------- Executable------------------#

//BEGIN_COMMENT
// Check for warning when unknown or incorrect trace category is provided.
//----------------------------------------------------------------------
//END_COMMENT

nop

// RUN: %clang %clangopts -c %s -o %t1.1.o
// RUN: %link %linkopts %t1.1.o --trace=1 -o %t.out 2>&1 | %filecheck %s
// RUN: %link %linkopts %t1.1.o --trace=fil -o %t.out 2>&1 | %filecheck %s
// RUN: %link %linkopts %t1.1.o --trace=all-sym -o %t.out 2>&1 | %filecheck %s

// CHECK: Warning: Unknown trace option
