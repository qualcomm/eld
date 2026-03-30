# REQUIRES: 64BitsArch

## A non-preemptible absolute symbol (e.g. --defsym in a PIE link) can
## be resolved at link time without a dynamic relocation. In a -shared
## link the symbol is preemptible and a dynamic reloc is expected.

# RUN: %clang %clangopts -c %s -o %t.o

## Shared link: symbol is preemptible, dynamic reloc expected.
# RUN: %link %linkopts -shared %t.o --defsym foo=0x20 -o %t.so
# RUN: %readelf -r %t.so | %filecheck %s --check-prefix=SHARED

## PIE link: symbol is non-preemptible, no dynamic reloc needed.
# RUN: %link %linkopts -pie %t.o --defsym foo=0x20 -o %t.pie
# RUN: %readelf -r %t.pie | %filecheck %s --check-prefix=PIE

# SHARED: foo
# PIE-NOT: foo

.data
.global bar
bar:
.quad foo
