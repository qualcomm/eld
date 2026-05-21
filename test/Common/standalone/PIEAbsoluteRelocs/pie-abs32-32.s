# REQUIRES: 32BitsArch

## 32-bit targets should reject absolute relocations in pie/shared with -z text.
## -z notext permits absolute relocations and emits TEXTREL.

# RUN: %clang %clangopts -c %s -o %t.global
# RUN: %clang %clangopts -c %s -Wa,-defsym,LOCAL=1 -o %t.local

# RUN: %not %link %linkopts -pie -z text %t.global -o %t.g.out 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -pie -z text %t.local -o %t.l.out 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -shared -z text %t.global -o %t.g.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR
# RUN: %not %link %linkopts -shared -z text %t.local -o %t.l.so 2>&1 \
# RUN:  | %filecheck %s --check-prefix=ERR

# RUN: %link %linkopts -pie -z notext %t.global -o %t.notext.global.pie
# RUN: %link %linkopts -pie -z notext %t.local -o %t.notext.local.pie

# RUN: %readelf -d %t.notext.global.pie | %filecheck %s --check-prefix=NOTEXT
# RUN: %readelf -d %t.notext.local.pie | %filecheck %s --check-prefix=NOTEXT

# ERR: recompile with -fPIC

# NOTEXT: TEXTREL

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
