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

namespace eld {
namespace ELF {
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

    These will be mapped to an internal vendor relocation for processing, or an
    error will be generated.
  */
  FirstNonstandardRelocation = 192,
  LastNonstandardRelocation = LastPublicRelocation,

  /*
    Internal Relaxation Relocation IDs for Linker Relaxations.

    These are used when we need to calculate a value or overwrite a field not
    represented by an existing relocation.

    Some of these use legacy names due to being part of the psABI in the past.
  */
  FirstRelaxationRelocation = 256,
  // Allocate 64 internal relocations, as that keeps the vendor relocations
  // in 64-item groups too.
  LastRelaxationRelocation = 319,

  R_RISCV_RVC_LUI = FirstRelaxationRelocation,
  R_RISCV_GPREL_I,
  R_RISCV_GPREL_S,
  R_RISCV_TPREL_I,
  R_RISCV_TPREL_S,

  /*
    Internal IDs for Nonstandard Relocations

    ELD internally uses an `int32_t` to represent relocation types, so we can
    use the IDs above 255 to represent relocations which we know how to
    process the R_RISCV_VENDOR symbol for.

    Internal Vendor Relocation IDs should not overlap, but do not have to use
    all 64 vendor relocations. `<vendor symbol>RelocationOffset` will be
    added to the nonstandard ID to get the internal ID.
  */
  FirstInternalRelocation = 320,
  LastInternalRelocation = 323,

  /* 'QUALCOMM' vendor relocations: 192-195 are represented by 320-323 */
  FirstQUALCOMMVendorRelocation = 320,
  LastQUALCOMMVendorRelocation = 323,
  QUALCOMMVendorRelocationOffset =
      FirstQUALCOMMVendorRelocation - FirstNonstandardRelocation,

#define ELF_RISCV_NONSTANDARD_RELOC(vendor_symbol, name, value)                \
  name = value + vendor_symbol##VendorRelocationOffset,
#include "llvm/BinaryFormat/ELFRelocs/RISCV_nonstandard.def"
#undef ELF_RISCV_NONSTANDARD_RELOC
};
} // namespace internal
} // namespace riscv
} // namespace ELF
} // namespace eld

#endif
