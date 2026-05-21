# REQUIRES: 64BitsArch

## 64-bit targets should reject absolute relocations in pie/shared
## regardless of -z text/notext.

# RUN: %clang %clangopts -c %s -o %t.global
# RUN: %clang %clangopts -c %s -Wa,-defsym,LOCAL=1 -o %t.local

# RUN: %not %link %linkopts -pie -z text %t.global -o %t.g.text 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -pie -z text %t.local -o %t.l.text 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -pie -z notext %t.global -o %t.g.notext 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -pie -z notext %t.local -o %t.l.notext 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR

# RUN: %not %link %linkopts -shared -z text %t.global -o %t.g.text.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -shared -z text %t.local -o %t.l.text.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -shared -z notext %t.global -o %t.g.notext.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -shared -z notext %t.local -o %t.l.notext.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR

# ERR: recompile with -fPIC

.ifdef LOCAL
.section .text.foo
foo:
  .word 100
.text
  .word foo
.else
.section .text.foo
.globl foo
foo:
  .word 100
.text
  .word foo
.endif
