#---PAuthRelocWeak.s------------ Executable------------------#
#BEGIN_COMMENT
# Test that linker handles weak undefined symbols with AUTH relocations.
# Per spec section 8.1.1: "if the target symbol is an undefined weak reference,
# the result of the relocation is 0 (nullptr) regardless of the signing schema"
#END_COMMENT
#START_TEST
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %llvm-mc -filetype=obj -triple=aarch64 main.s -o main.o

# Test PIE executable with weak undefined symbols
# RUN: %link %linkopts main.o -pie -o main.pie
# RUN: %readelf --elf-output-style LLVM -r main.pie 2>&1 | %filecheck %s --check-prefix=PIE-RELOCS --allow-empty
# RUN: %readelf -x .data main.pie | %filecheck %s --check-prefix=PIE-DATA

# No dynamic relocations should be emitted for weak undefined symbols
# PIE-RELOCS-NOT: weak_undef

# All weak undefined AUTH relocations should resolve to 0
# PIE-DATA:      Hex dump of section '.data':
# PIE-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000 00000000 00000000
##                               ^^^^^^^^ ^^^^^^^^ weak_undef_func@AUTH(da,42) = 0
##                                                 ^^^^^^^^ ^^^^^^^^ weak_undef_data@AUTH(ia,43) = 0
# PIE-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000
##                               ^^^^^^^^ ^^^^^^^^ (weak_undef_func + 0x1234)@AUTH(da,44) = 0

# Test non-PIE executable
# RUN: %link %linkopts main.o -no-pie -o main.nopie
# RUN: %readelf --elf-output-style LLVM -r main.nopie 2>&1 | %filecheck %s --check-prefix=NOPIE-RELOCS --allow-empty
# RUN: %readelf -x .data main.nopie | %filecheck %s --check-prefix=NOPIE-DATA

# NOPIE-RELOCS-NOT: weak_undef

# NOPIE-DATA:      Hex dump of section '.data':
# NOPIE-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000 00000000 00000000
# NOPIE-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000

# Test static executable
# RUN: %link %linkopts -static main.o -pie -o main.static
# RUN: %readelf --elf-output-style LLVM -r main.static 2>&1 | %filecheck %s --check-prefix=STATIC-RELOCS --allow-empty
# RUN: %readelf -x .data main.static | %filecheck %s --check-prefix=STATIC-DATA

# STATIC-RELOCS-NOT: weak_undef

# STATIC-DATA:      Hex dump of section '.data':
# STATIC-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000 00000000 00000000
# STATIC-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000

# Test that weak DEFINED symbols work normally (not resolved to 0)
# RUN: %llvm-mc -filetype=obj -triple=aarch64 weak_def.s -o weak_def.o
# RUN: %link %linkopts weak_def.o -pie -o weak_def
# RUN: %readelf --elf-output-style LLVM -r weak_def 2>&1 | %filecheck %s --check-prefix=WEAK-DEF-RELOCS
# RUN: %readelf -x .data weak_def | %filecheck %s --check-prefix=WEAK-DEF-DATA

# Weak defined symbols should generate AUTH_RELATIVE relocations
# WEAK-DEF-RELOCS:      Section ({{.+}}) .rela.dyn {
# WEAK-DEF-RELOCS:      {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# WEAK-DEF-RELOCS:      }

# Weak defined symbols should have signing schema encoded
# WEAK-DEF-DATA:      Hex dump of section '.data':
# WEAK-DEF-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 2a000020
##                                    ^^^^^^^^ No implicit val (rela reloc)
##                                             ^^^^ Discr = 42
##                                                   ^^ Key = DA

#END_TEST

#--- main.s
.global _start
.type _start, @function
_start:
  ret

# Declare weak undefined symbols
.weak weak_undef_func
.weak weak_undef_data

.section .data
.p2align 3

# Test weak undefined with AUTH relocations
# Should resolve to 0 regardless of signing schema
.quad weak_undef_func@AUTH(da,42)
.quad weak_undef_data@AUTH(ia,43)

# Test with non-zero addend - should still resolve to 0
.quad (weak_undef_func + 0x1234)@AUTH(da,44)

#--- weak_def.s
.global _start
.type _start, @function
_start:
  ret

# Declare weak DEFINED symbol (not undefined)
# This should NOT resolve to 0
.weak weak_def_func
.type weak_def_func, @function
weak_def_func:
  ret

.section .data
.p2align 3

# Should generate AUTH_RELATIVE relocation, not resolve to 0
.quad weak_def_func@AUTH(da,42)
