# REQUIRES: arm
# RUN: %llvm-mc -filetype=obj --arm-add-build-attributes \
# RUN:   -triple=armv7a-none-linux-gnueabi %s -o %t.o
# RUN: %link %linkopts %t.o --defsym=__gxx_personality_v0=0 \
# RUN:   --defsym=__aeabi_unwind_cpp_pr0=0 -o %t.out
# RUN: %objdump -s %t.out | %filecheck %s
# RUN: %link %linkopts --no-merge-exidx-entries %t.o \
# RUN:   --defsym=__gxx_personality_v0=0 \
# RUN:   --defsym=__aeabi_unwind_cpp_pr0=0 -o %t.no-merge.out
# RUN: %readelf -S -W %t.no-merge.out | %filecheck %s --check-prefix=NO-MERGE

## Ported from lld's arm-exidx-dedup.s. Consecutive EXIDX entries with the same
## inline unwind word are merged. Entries that reference .ARM.extab are not
## merged, even when their extab contents are identical.

# CHECK: Contents of section .ARM.exidx:
# CHECK-NEXT: 0024 dcffff7f 01000000 e4ffff7f 08849780
# CHECK-NEXT: 0034 e8ffff7f 14000000 e4ffff7f 18000000
# CHECK-NEXT: 0044 e0ffff7f 01000000
# CHECK-NEXT: Contents of section .ARM.extab:
# NO-MERGE: .ARM.exidx{{[[:space:]]+}}ARM_EXIDX{{[[:space:]]}}{{.*}}000050

        .syntax unified

        .section .text.00, "ax", %progbits
        .globl _start
_start:
        .fnstart
        bx lr
        .cantunwind
        .fnend

        .section .text.01, "ax", %progbits
        .globl f1
f1:
        .fnstart
        bx lr
        .cantunwind
        .fnend

        .section .text.02, "ax", %progbits
        .globl f2
f2:
        .fnstart
        bx lr
        .cantunwind
        .fnend

        .globl f3
f3:
        .fnstart
        bx lr
        .cantunwind
        .fnend

        .section .text.03, "ax", %progbits
        .global f4
f4:
        .fnstart
        bx lr
        .save {r7, lr}
        .setfp r7, sp, #0
        .fnend

        .section .text.04, "ax", %progbits
        .global f5
f5:
        .fnstart
        bx lr
        .save {r7, lr}
        .setfp r7, sp, #0
        .fnend

        .global f6
f6:
        .fnstart
        bx lr
        .save {r7, lr}
        .setfp r7, sp, #0
        .fnend

        .section .text.05, "ax",%progbits
        .global f7
f7:
        .fnstart
        bx lr
        .personality __gxx_personality_v0
        .handlerdata
        .long 0
        .fnend

        .section .text.06, "ax",%progbits
        .global f8
f8:
        .fnstart
        bx lr
        .personality __gxx_personality_v0
        .handlerdata
        .long 0
        .fnend
