//===- RISCVRelocationInternal.h-------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===- RISCVRelocationInternal.h - Internal Relocations RISCV -------------===//
//
// (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//
#ifndef RISCV_RELOCATION_INTERNAL_H
#define RISCV_RELOCATION_INTERNAL_H

#include <cstdint>

#ifndef ELF_RELOC
#define ELF_RELOC(name, value) name = value,
#endif

namespace eld {
namespace ELF {
enum {
  /* Legacy Binutils Relaxation Relocation Names */
  // clang-format off
  ELF_RELOC(R_RISCV_RVC_LUI, 46)
  ELF_RELOC(R_RISCV_GPREL_I, 47)
  ELF_RELOC(R_RISCV_GPREL_S, 48)
  ELF_RELOC(R_RISCV_TPREL_I, 49)
  ELF_RELOC(R_RISCV_TPREL_S, 50)
  // clang-format on
};

namespace riscv {
namespace internal {
enum : uint32_t {

  /* RISC-V psABI relocation IDs are 0-255 (both rv32 and rv64) */
  FirstPublicRelocation = 0,
  LastPublicRelocation = 255,

  /*
    The ABI relocation space of 192-255 is reserved for nonstandard ABI
    extensions, which must be accompanied by a R_RISCV_VENDOR relocation to
    identify which vendor has defined the relocation operation.
  */
  FirstNonstandardRelocation = 192,
  LastNonstandardRelocation = LastPublicRelocation,

  /*
    Internal IDs for Nonstandard Relocations

    ELD internally uses an `int32_t` to represent relocation types, so we can
    use the IDs above 255 to represent relocations for which we know how to
    process the R_RISCV_VENDOR symbol for.

    Internal Vendor Relocation IDs should not overlap, but do not have to use
    all 64 vendor relocations. `<vendor symbol>RelocationOffset` will be
    added to the nonstandard ID to get the internal ID.
  */
  FirstInternalRelocation = 256,
  LastInternalRelocation = 259,

  /* 'QUALCOMM' vendor relocations: 192-195 are represented by 256-259 */
  FirstQUALCOMMVendorRelocation = 256,
  LastQUALCOMMVendorRelocation = 259,
  QUALCOMMVendorRelocationOffset =
      FirstQUALCOMMVendorRelocation - FirstNonstandardRelocation,

#define ELF_RISCV_NONSTANDARD_RELOC(vendor_symbol, name, value)                \
  name = value + vendor_symbol##VendorRelocationOffset,
#include "llvm/BinaryFormat/ELFRelocs/RISCV_nonstandard.def"
#undef ELF_RISCV_NONSTANDARD_RELOC

  // TODO: Internal Relaxation Relocation IDs for relaxations which require
  // calculating a value or overwriting a field not represented by a relocation.
};
} // namespace internal
} // namespace riscv
} // namespace ELF
} // namespace eld

#endif
