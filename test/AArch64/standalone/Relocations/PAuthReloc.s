#---PAuthReloc.s--------------------- Executable------------------#
#BEGIN_COMMENT
# Test that linker handles signed relocations (pauth).
#END_COMMENT
#START_TEST
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %llvm-mc -filetype=obj -triple=aarch64 a.s -o a.o
# RUN: %link %linkopts -shared a.o -o a.so
# RUN: %llvm-mc -filetype=obj -triple=aarch64 main.s -o main.o

# Test PAuth relocations with PIE
# RUN: %link %linkopts main.o -pie a.so -o main
# RUN: %readelf --elf-output-style LLVM -r main 2>&1 | %filecheck %s --check-prefix=UNPACKED

# UNPACKED:          Section ({{.+}}) .rela.dyn {
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFFFFFF{{[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x12345{{[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x34567{{[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFCBA98{{[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 bar 0x64
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 bar 0x0
# UNPACKED-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 foo 0x1111
# UNPACKED-NEXT:     }

# Validate relocation place (and signing schema) in section data
# RUN: %readelf -x .test main | %filecheck %s --check-prefix=HEX

# HEX: Hex dump of section '.test':
# HEX-NEXT: {{0x[0-9a-f]+}} 00000000 2a000020 00000000 2b000000
##                          ^^^^^^^^ No implicit val (rela reloc)
##                                   ^^^^ Discr = 42
##                                         ^^ Key = DA
##                                            ^^^^^^^^ No implicit val (rela reloc)
##                                                     ^^^^ Discr = 43
##                                                           ^^ Key = IA
# HEX-NEXT: {{0x[0-9a-f]+}} 00000000 2c000080 00000000 2d000020
##                          ^^^^^^^^ No implicit val (rela reloc)
##                                   ^^^^ Discr = 44
##                                         ^^ Key = IA, Addr
##                                            ^^^^^^^^ No implicit val (rela reloc)
##                                                     ^^^^ Discr = 45
##                                                           ^^ Key = DA
# HEX-NEXT: {{0x[0-9a-f]+}} 00000000 2e000020 00000000 2f000020
##                          ^^^^^^^^ No implicit val (rela reloc)
##                                   ^^^^ Discr = 46
##                                         ^^ Key = DA
##                                            ^^^^^^^^ No implicit val (rela reloc)
##                                                     ^^^^ Discr = 47
##                                                           ^^ Key = DA
# HEX-NEXT: {{0x[0-9a-f]+}} 00000000 30000020 00000000 31000020
##                          ^^^^^^^^ No implicit val (rela reloc)
##                                   ^^^^ Discr = 48
##                                         ^^ Key = DA
##                                            ^^^^^^^^ No implicit val (preemptible abs reloc)
##                                                     ^^^^ Discr = 49
##                                                           ^^ Key = DA
# HEX-NEXT: {{0x[0-9a-f]+}} 00000000 32000000 77000000 00330000
##                          ^^^^^^^^ No implicit val (preemptible abs reloc)
##                                   ^^^^ Discr = 50
##                                         ^^ Key = IA
##                                            ^^ padding byte
##                                              ^^^^^^ ^^ No implicit val (rela reloc)
##                                                       ^^^^ Discr = 51
# HEX-NEXT: {{0x[0-9a-f]+}} 20770000 00003400 0020
##                          ^^ Key = DA
##                            ^^ padding byte
##                              ^^^^ ^^^^ No implicit val (rela reloc)
##                                       ^^^^ Discr = 52
##                                              ^^ Key = DA

# Test PAuth relocations in read-only section
# RUN: %readelf -x .rodata main | %filecheck %s --check-prefix=RODATA

# RODATA: Hex dump of section '.rodata':
# RODATA-NEXT: {{0x[0-9a-f]+}} 00000000 35000020 00000000 36000000
##                             ^^^^^^^^ No implicit val (rela reloc)
##                                      ^^^^ Discr = 53
##                                            ^^ Key = DA
##                                               ^^^^^^^^ No implicit val (rela reloc)
##                                                        ^^^^ Discr = 54
##                                                              ^^ Key = IA
# RODATA-NEXT: {{0x[0-9a-f]+}} 00000000 37000020
##                             ^^^^^^^^ No implicit val (rela reloc)
##                                      ^^^^ Discr = 55
##                                            ^^ Key = DA
# Test PAuth relocations without PIE
# RUN: %link %linkopts main.o -no-pie a.so -o main.nopie
# RUN: %readelf --elf-output-style LLVM -r main.nopie 2>&1 | %filecheck %s --check-prefix=NOPIE

# NOPIE:      Section ({{.+}}) .rela.dyn {
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFFFFFF{{[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x12345{{[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x34567{{[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFCBA98{{[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 bar 0x64
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 bar 0x0
# NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_ABS64 foo 0x1111
# NOPIE-NEXT: }

# Test PAuth relocations with static executable (PIE)
# RUN: %link %linkopts -static main.o -pie a.o -o main.static
# RUN: %readelf --elf-output-style LLVM -r main.static 2>&1 | %filecheck %s --check-prefix=STATIC

# STATIC:      Section ({{.+}}) .rela.dyn {
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFFFFFF{{[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x12345{{[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0x34567{{[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - 0xFFFFFFFFCBA98{{[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC-NEXT: }

# Test PAuth relocations with static executable (without PIE)
# RUN: %link %linkopts -static main.o -no-pie a.o -o main.static.nopie
# RUN: %readelf --elf-output-style LLVM -r main.static.nopie 2>&1 | %filecheck %s --check-prefix=STATIC_NOPIE

# STATIC_NOPIE:      Section ({{.+}}) .rela.dyn {
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT:       {{0x[0-9A-F]+}} R_AARCH64_AUTH_RELATIVE - {{0x[0-9A-F]+}}
# STATIC_NOPIE-NEXT: }

#END_TEST

#--- a.s
.global bar
.type bar, @function
bar:

.global foo
foo:

#--- main.s
.section .test, "aw"
.p2align 3
.quad (__etext + 1)@AUTH(da,42)
.quad (.test + 2)@AUTH(ia,43)
.quad (__etext - 1000)@AUTH(ia,44,addr)
.quad (__etext + 0x12345678)@AUTH(da,45)
## Addend wider than 32 bits, not enough room for storing implicitly, would go to rela
.quad (__etext + 0x123456789A)@AUTH(da,46)
## Negative addend wider than 32 bits, not enough room for storing implicitly, would go to rela
.quad (__etext - 0x123456789A)@AUTH(da,47)
## INT32_MAX plus non-zero .test is wider than 32 bits, not enough room for storing implicitly, would go to rela
.quad (.test + 0x7FFFFFFF)@AUTH(da,48)
.quad (foo + 0x1111)@AUTH(da,49)
.quad bar@AUTH(ia,50)
.byte 0x77
.quad (__etext + 4)@AUTH(da,51)
.byte 0x77
.quad (.test + 5)@AUTH(da,52)

.section .rodata, "a"
.p2align 3
.quad (__etext + 10)@AUTH(da,53)
.quad (.rodata + 8)@AUTH(ia,54)
.quad (bar + 100)@AUTH(da,55)
