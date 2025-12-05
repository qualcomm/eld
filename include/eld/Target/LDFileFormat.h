//===- LDFileFormat.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_LDFILEFORMAT_H
#define ELD_TARGET_LDFILEFORMAT_H
#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace eld {
class LinkerConfig;

/** \class LDFileFormat
 *  \brief LDFileFormat describes the common file formats.
 */
class LDFileFormat {
public:
  enum Kind : uint8_t {
    Common,
    Debug,
    Discard,
    DynamicRelocation,
    EhFrame,
    EhFrameHdr,
    Error,
    Exclude,
    GCCExceptTable,
    GNUProperty,
    Group,
    Ignore,
    Internal,
    LinkOnce,
    MergeStr,
    MetaData,
    NamePool,
    Note,
    Null,
    OutputSectData,
    Regular,
    Relocation,
    StackNote,
    SymbolVersion,
    Target,
    Timing,
    Version,
  };

  static Kind getELFSectionKind(uint32_t Flags, uint32_t AddrAlign,
                                uint32_t EntSize, uint32_t Type,
                                llvm::StringRef Name,
                                const LinkerConfig &Config);

protected:
  LDFileFormat() {}

public:
  virtual ~LDFileFormat() {}
};
} // namespace eld

#endif
