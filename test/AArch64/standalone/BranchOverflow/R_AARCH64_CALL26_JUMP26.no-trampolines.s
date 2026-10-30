## With --no-trampolines no branch island is created, so an out-of-range
## R_AARCH64_CALL26 or R_AARCH64_JUMP26 reaches the 28-bit range check. The
## aligned values at the positive and negative limits link; one instruction
## beyond either limit must be reported, not truncated.

# RUN: split-file %s %t
# RUN: %llvm-mc -filetype=obj -triple=aarch64 %t/call.s -o %t/call.o
# RUN: %llvm-mc -filetype=obj -triple=aarch64 %t/jump.s -o %t/jump.o
# RUN: %link %linkopts -march aarch64 --no-trampolines --defsym bar=0x7FFFFFC %t/call.o -o %t/call-in.out
# RUN: %link %linkopts -march aarch64 --no-trampolines --defsym bar=0x7FFFFFC %t/jump.o -o %t/jump-in.out
# RUN: not %link %linkopts -march aarch64 --no-trampolines --defsym bar=0x8000000 %t/call.o -o %t/call-out.out 2>&1 | %filecheck %s --check-prefix=CALL
# RUN: not %link %linkopts -march aarch64 --no-trampolines --defsym bar=0x8000000 %t/jump.o -o %t/jump-out.out 2>&1 | %filecheck %s --check-prefix=JUMP
# RUN: %link %linkopts -march aarch64 --no-trampolines --defsym bar=0xfffffffff8000000 %t/call.o -o %t/call-negative-in.out
# RUN: %link %linkopts -march aarch64 --no-trampolines --defsym bar=0xfffffffff8000000 %t/jump.o -o %t/jump-negative-in.out
# RUN: not %link %linkopts -march aarch64 --no-trampolines --defsym bar=0xfffffffff7fffffc %t/call.o -o %t/call-negative-out.out 2>&1 | %filecheck %s --check-prefix=CALL-NEGATIVE
# RUN: not %link %linkopts -march aarch64 --no-trampolines --defsym bar=0xfffffffff7fffffc %t/jump.o -o %t/jump-negative-out.out 2>&1 | %filecheck %s --check-prefix=JUMP-NEGATIVE

# CALL: relocation R_AARCH64_CALL26 out of range: 134217728 is not in [-134217728, 134217727]
# JUMP: relocation R_AARCH64_JUMP26 out of range: 134217728 is not in [-134217728, 134217727]
# CALL-NEGATIVE: relocation R_AARCH64_CALL26 out of range: -134217732 is not in [-134217728, 134217727]
# JUMP-NEGATIVE: relocation R_AARCH64_JUMP26 out of range: -134217732 is not in [-134217728, 134217727]

#--- call.s
	.text
	.globl	foo
	.p2align	2
foo:
	bl	bar
	ret

#--- jump.s
	.text
	.globl	baz
	.p2align	2
baz:
	b	bar
