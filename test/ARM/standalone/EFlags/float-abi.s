// Check that output float ABI e_flags are derived from Tag_ABI_VFP_args
// instead of being inherited from input e_flags.
//
// For EABI5 and later executable/shared outputs, match ld.bfd:
//   - Default/Base/ToolChain -> EF_ARM_ABI_FLOAT_SOFT
//   - VFP                    -> EF_ARM_ABI_FLOAT_HARD
//   - Compatible             -> preserve the current/default convention
//
// Relocatable and pre-EABI5 outputs do not gain the float ABI bits.

// RUN: split-file %s %t

// Assemble EABI5 relocatable objects. Their input e_flags do not contain
// the float ABI bits.
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %t/none.s -o %t/none.o
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %t/base.s -o %t/base.o
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %t/hard.s -o %t/hard.o
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %t/toolchain.s -o %t/toolchain.o
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %t/compat.s -o %t/compat.o

// Also cover an object with no .ARM.attributes section at all.
// RUN: %objcopy --remove-section=.ARM.attributes %t/none.o %t/noattrs.o

// Verify that HARD is not already present in the input e_flags.
// RUN: %readelf -h %t/hard.o | %filecheck %s --check-prefix=INPUT5

// EABI5 output behavior.
// RUN: %link %emulation %t/noattrs.o -o %t/noattrs5.out
// RUN: %readelf -h %t/noattrs5.out | %filecheck %s --check-prefix=SOFT5
// RUN: %link %emulation %t/none.o -o %t/none5.out
// RUN: %readelf -h %t/none5.out | %filecheck %s --check-prefix=SOFT5
// RUN: %link %emulation %t/base.o -o %t/base5.out
// RUN: %readelf -h %t/base5.out | %filecheck %s --check-prefix=SOFT5
// RUN: %link %emulation %t/hard.o -o %t/hard5.out
// RUN: %readelf -h %t/hard5.out | %filecheck %s --check-prefix=HARD5

// Float ABI bits are only valid on executable and shared outputs.
// RUN: %link %emulation -shared %t/hard.o -o %t/hard5.so
// RUN: %readelf -h %t/hard5.so | %filecheck %s --check-prefix=HARD5-DYN
// RUN: %link %emulation -r %t/hard.o -o %t/hard5.rel
// RUN: %readelf -h %t/hard5.rel | %filecheck %s --check-prefix=REL5

// The relocatable output keeps Tag_ABI_VFP_args, so a later link derives
// the bit again.
// RUN: %link %emulation %t/hard5.rel -o %t/hard5rel.out
// RUN: %readelf -h %t/hard5rel.out | %filecheck %s --check-prefix=HARD5

// Binary-only links have no EABI version, so no float ABI bit is added.
// RUN: %link %emulation --format=binary %t/none.s -o %t/binary.out
// RUN: %readelf -h %t/binary.out | %filecheck %s --check-prefix=BINONLY

// RUN: %link %emulation %t/toolchain.o -o %t/toolchain5.out
// RUN: %readelf -h %t/toolchain5.out | %filecheck %s --check-prefix=SOFT5
// RUN: %link %emulation %t/compat.o -o %t/compat5.out
// RUN: %readelf -h %t/compat5.out | %filecheck %s --check-prefix=SOFT5

// CompatibleFPAAPCS must not reset an already selected hard-float ABI.
// RUN: %link %emulation %t/hard.o %t/compat.o -o %t/hard-compat5.out
// RUN: %readelf -h %t/hard-compat5.out | %filecheck %s --check-prefix=HARD5

// Rewrite only the ELF e_flags version field so the same attribute inputs can
// exercise EABI4 output behavior.
// RUN: %python %t/set-eabi.py 4 \
// RUN:   %t/noattrs.o %t/noattrs4.o \
// RUN:   %t/none.o %t/none4.o \
// RUN:   %t/base.o %t/base4.o \
// RUN:   %t/hard.o %t/hard4.o \
// RUN:   %t/toolchain.o %t/toolchain4.o \
// RUN:   %t/compat.o %t/compat4.o

// EABI4 must not gain the EABI5 float ABI bits.
// RUN: %link %emulation %t/noattrs4.o -o %t/noattrs4.out
// RUN: %readelf -h %t/noattrs4.out | %filecheck %s --check-prefix=EABI4
// RUN: %link %emulation %t/none4.o -o %t/none4.out
// RUN: %readelf -h %t/none4.out | %filecheck %s --check-prefix=EABI4
// RUN: %link %emulation %t/base4.o -o %t/base4.out
// RUN: %readelf -h %t/base4.out | %filecheck %s --check-prefix=EABI4
// RUN: %link %emulation %t/hard4.o -o %t/hard4.out
// RUN: %readelf -h %t/hard4.out | %filecheck %s --check-prefix=EABI4
// RUN: %link %emulation %t/toolchain4.o -o %t/toolchain4.out
// RUN: %readelf -h %t/toolchain4.out | %filecheck %s --check-prefix=EABI4
// RUN: %link %emulation %t/compat4.o -o %t/compat4.out
// RUN: %readelf -h %t/compat4.out | %filecheck %s --check-prefix=EABI4

// INPUT5: Flags: 0x5000000
// SOFT5:  Flags: 0x5000200
// HARD5:  Flags: 0x5000400
// HARD5-DYN: Type: DYN
// HARD5-DYN: Flags: 0x5000400
// REL5:      Type: REL
// REL5:      Flags: 0x5000000
// BINONLY:   Flags: 0x0
// EABI4:  Flags: 0x4000000

//--- none.s
.text
.syntax unified
.globl none_entry
none_entry:
  bx lr

//--- base.s
.text
.syntax unified
.eabi_attribute 28, 0
.globl base_entry
base_entry:
  bx lr

//--- hard.s
.text
.syntax unified
.eabi_attribute 28, 1
.globl hard_entry
hard_entry:
  bx lr

//--- toolchain.s
.text
.syntax unified
.eabi_attribute 28, 2
.globl toolchain_entry
toolchain_entry:
  bx lr

//--- compat.s
.text
.syntax unified
.eabi_attribute 28, 3
.globl compat_entry
compat_entry:
  bx lr

//--- set-eabi.py
from pathlib import Path
import struct
import sys

version = int(sys.argv[1])
flags = version << 24

args = sys.argv[2:]
assert len(args) % 2 == 0

for src, dst in zip(args[0::2], args[1::2]):
    data = bytearray(Path(src).read_bytes())
    struct.pack_into("<I", data, 36, flags)
    Path(dst).write_bytes(data)
