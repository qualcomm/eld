

# RUN: llvm-mc -filetype=obj -triple=armv7-linux-musl %s -o %t.o
# RUN: %link --gc-sections %t.o -o %t
# RUN: llvm-readelf -S -r %t | %filecheck %s

# Test that we discard R_ARM_NONE, but respect the references it creates among
# sections.

# CHECK: .data
# CHECK: There are no relocations in this file.

# RUN: %link -r %t.o -o %t
# RUN: llvm-readobj -r %t | %filecheck --check-prefix=RELOC %s

# RELOC:      Section ({{.*}}) .rel.text {
# RELOC-NEXT:   0x0 R_ARM_NONE .data
# RELOC-NEXT: }

.globl _start
_start:
  nop
  .reloc 0, R_ARM_NONE, .data

.data
  .long 0
