#---PAuthRelocDebug.s------------ Executable------------------#
#BEGIN_COMMENT
# Test that linker rejects AUTH relocations when referred from non-allocated sections.
# AUTH relocations require runtime signing by the dynamic linker, which only
# processes allocated sections. Non-allocated sections like .debug_* cannot
# have AUTH relocations.
#END_COMMENT
#START_TEST
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %llvm-mc -filetype=obj -triple=aarch64 a.s -o a.o
# RUN: %link %linkopts -shared a.o -o a.so
# RUN: %llvm-mc -filetype=obj -triple=aarch64 main.s -o main.o

# Test PIE executable with AUTH relocations in debug section (should fail)
# RUN: %not %link %linkopts main.o -pie a.so -o main 2>&1 | %filecheck %s --check-prefix=PIE

# PIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `foo' cannot be used when referred from non-allocated section `.rela.debug_info'
# PIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `bar' cannot be used when referred from non-allocated section `.rela.debug_info'
# PIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `.text' cannot be used when referred from non-allocated section `.rela.debug_info'

# Test non-PIE executable with AUTH relocations in debug section (should also fail)
# RUN: %not %link %linkopts main.o -no-pie a.so -o main.nopie 2>&1 | %filecheck %s --check-prefix=NOPIE

# NOPIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `foo' cannot be used when referred from non-allocated section `.rela.debug_info'
# NOPIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `bar' cannot be used when referred from non-allocated section `.rela.debug_info'
# NOPIE: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `.text' cannot be used when referred from non-allocated section `.rela.debug_info'

# Test static executable with AUTH relocations in debug section (should also fail)
# RUN: %not %link %linkopts -static main.o a.o -o main.static 2>&1 | %filecheck %s --check-prefix=STATIC

# STATIC: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `foo' cannot be used when referred from non-allocated section `.rela.debug_info'
# STATIC: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `bar' cannot be used when referred from non-allocated section `.rela.debug_info'
# STATIC: Fatal: relocation type `R_AARCH64_AUTH_ABS64' for symbol `.text' cannot be used when referred from non-allocated section `.rela.debug_info'

# Verify that regular (non-AUTH) relocations in debug sections still work
# RUN: %llvm-mc -filetype=obj -triple=aarch64 main_regular.s -o main_regular.o
# RUN: %link %linkopts main_regular.o -pie a.so -o main.regular
# RUN: %readelf -r main.regular | %filecheck %s --check-prefix=REGULAR --allow-empty

# Debug relocations are resolved statically, so no dynamic relocations should appear
# REGULAR-NOT: .debug_info

#END_TEST

#--- a.s
.global bar
.type bar, @function
bar:
  ret

.global foo
.data
foo:
  .word 42

#--- main.s
.global _start
.type _start, @function
_start:
  ret

baz:
  nop

# AUTH relocations in non-allocated debug section (should be rejected)
.section .debug_info, "", @progbits
.p2align 3
.quad foo@AUTH(da,42)
.quad bar@AUTH(ia,43)
.quad baz@AUTH(da,44)

# AUTH relocations in allocated sections should work fine
.section .data
.p2align 3
.quad foo@AUTH(da,45)
.quad bar@AUTH(ia,46)
.quad baz@AUTH(da,47)

#--- main_regular.s
.global _start
.type _start, @function
_start:
  ret

baz:
  nop

# Regular (non-AUTH) relocations in debug sections are fine
.section .debug_info, "", @progbits
.p2align 3
.quad foo
.quad bar
.quad baz

.section .data
.p2align 3
.quad foo
.quad bar
.quad baz
