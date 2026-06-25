#---ABS64PLTAddr.s----------------- Executable ------------------#
#BEGIN_COMMENT
# Regression test for R_AARCH64_ABS64 to a preemptible function.
#
# When a non-PIE executable references a function defined in a shared
# library through R_AARCH64_ABS64 stored in a writable section (e.g. a
# function pointer in .data), the linker sets ReservePLT for the symbol
# but does NOT reserve a dynamic relocation (see
# GNULDBackend::symbolNeedsDynRel, "pSymHasPLT && Function -> false").
# The dynamic linker therefore never revisits that field, and the value
# baked in at link time must be the PLT entry address, not 0 and not
# the raw addend.
#
#END_COMMENT
#START_TEST
# RUN: rm -rf %t && split-file %s %t && cd %t

# RUN: %llvm-mc -filetype=obj -triple=aarch64 a.s -o a.o
# RUN: %link %linkopts -shared a.o -soname=a.so -o a.so
# RUN: %llvm-mc -filetype=obj -triple=aarch64 main.s -o main.o
# RUN: %llvm-mc -filetype=obj -triple=aarch64 weak.s -o weak.o

## Non-PIE dynamic executable: R_AARCH64_ABS64 to preemptible function `foo`
## must be resolved at link time to `foo`'s PLT entry address. Pin the
## section VAs so the expected PLT VA (0x40020 for the first PLTN entry
## sitting after the 32-byte PLT0 header) is a fixed literal below.
# RUN: %link %linkopts main.o a.so -no-pie \
# RUN:   --section-start .text=0x10000 \
# RUN:   --section-start .data=0x20000 \
# RUN:   --section-start .plt=0x40000 \
# RUN:   -o main.nopie
# RUN: %readelf --elf-output-style LLVM -r main.nopie 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NOPIE-RELOCS --allow-empty
# RUN: %readelf -x .data main.nopie \
# RUN:   | %filecheck %s --check-prefix=NOPIE-DATA

## There must be NO dynamic relocation that would fix up the .data field
## for `foo` at runtime (i.e. no R_AARCH64_ABS64 / R_AARCH64_GLOB_DAT
## naming foo). The JUMP_SLOT in .rela.plt is expected, it populates
## .got.plt so the PLT stub can resolve foo lazily. The regression this
## test guards against is the linker forgetting to bake foo's PLT
## address into the .data slot at link time, since without a matching
## dyn reloc there is no runtime fixup for that word.
# NOPIE-RELOCS: .rela.plt
# NOPIE-RELOCS: R_AARCH64_JUMP_SLOT foo
# NOPIE-RELOCS-NOT: R_AARCH64_ABS64 foo
# NOPIE-RELOCS-NOT: R_AARCH64_GLOB_DAT foo

## Both slots must hold `foo`'s PLT entry VA (0x40020), byte-swapped
## for little endian. Slot 1: `.quad foo`         -> 0x0000000000040020
## Slot 2: `.quad foo + 0x1000` -> 0x0000000000041020
# NOPIE-DATA:      Hex dump of section '.data':
# NOPIE-DATA-NEXT: {{0x[0-9a-f]+}} 20000400 00000000 20100400 00000000

## Plain-ABS64 to a weak undefined symbol in a non-PIE exec: exercises the
## `isWeakUndef() && Exec` S=0 branch in abs(). Both eight-byte slots must
## be zero.
# RUN: %link %linkopts weak.o -no-pie -o weak.nopie
# RUN: %readelf --elf-output-style LLVM -r weak.nopie 2>&1 \
# RUN:   | %filecheck %s --check-prefix=WEAK-RELOCS --allow-empty
# RUN: %readelf -x .data weak.nopie \
# RUN:   | %filecheck %s --check-prefix=WEAK-DATA

# WEAK-RELOCS-NOT: weak_undef

# WEAK-DATA:      Hex dump of section '.data':
# WEAK-DATA-NEXT: {{0x[0-9a-f]+}} 00000000 00000000 00010000 00000000
##                               ^^^^^^^^ ^^^^^^^^ weak_undef = 0 (S=0, A=0)
##                                                 ^^^^^^^^ ^^^^^^^^ weak_undef + 0x100 = 0x100 (S=0, A=0x100)

#END_TEST

#--- a.s
.global foo
.type foo, @function
foo:
  ret

#--- main.s
.global _start
.type _start, @function
_start:
  ret

.section .data, "aw"
.p2align 3
## R_AARCH64_ABS64 against a preemptible function. The linker reserves a
## PLT entry for `foo` but no dyn reloc for this field, so the field must
## be baked to the PLT entry address at link time.
.quad foo
.quad foo + 0x1000

#--- weak.s
.global _start
.type _start, @function
_start:
  ret

.weak weak_undef
.type weak_undef, @function

.section .data, "aw"
.p2align 3
## Plain R_AARCH64_ABS64 to a weak undefined symbol. Non-PIE exec: the
## `isWeakUndef() && Exec` branch in abs() zeroes S, and the field must
## be written as zero.
.quad weak_undef
.quad weak_undef + 0x100
