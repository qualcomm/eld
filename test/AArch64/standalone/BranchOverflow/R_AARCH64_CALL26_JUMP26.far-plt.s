## A far PLT entry must be reached through a range-extension trampoline.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=aarch64 %t/caller.s -o %t/caller.o
# RUN: %link %linkopts -march aarch64 -shared -T %t/layout.lds --trace=trampolines %t/caller.o -o %t/out.so 2>&1 | %filecheck %s

# CHECK: trampoline_for_external_function

#--- caller.s
.text
.globl caller
.type caller, %function
caller:
  bl external_function
  ret

#--- layout.lds
SECTIONS {
  .text 0x1000 : { *(.text*) }
  .plt 0x09000000 : { *(.plt*) }
  .got 0x09001000 : { *(.got) }
  .got.plt 0x09002000 : { *(.got.plt) }
}
