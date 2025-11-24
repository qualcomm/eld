// RUN: %clang %clangopts -c -flto %s -o %t.o
// RUN: %link %linkopts %t.o -o %t.out -plugin-opt=-print-pipeline-passes | %filecheck %s

// CHECK: pipeline-passes:

int main() {
  return 5;
}
