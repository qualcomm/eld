//===- ZOption.h-----------------------------------------------------------===//
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

#ifndef ELD_INPUT_ZOPTION_H
#define ELD_INPUT_ZOPTION_H

#include "llvm/Support/DataTypes.h"

namespace eld {

/** \class ZOption
 *  \brief The -z options for GNU ld compatibility.
 */
class ZOption {
public:
  enum ZOptionKind {
    CombReloc,
    CommPageSize,
    Defs,
    ExecStack,
    Global,
    InitFirst,
    InterPose,
    Lazy,
    LoadFltr,
    MaxPageSize,
    MulDefs,
    NoCombReloc,
    NoCopyReloc,
    NoDefaultLib,
    NoDelete,
    NoExecStack,
    NoGnuStack,
    NoRelro,
    Now,
    Origin,
    Relro,
    Text,
    CompactDyn,
    ForceBTI,
    ForcePACPLT,
    Unknown
  };

public:
  ZOption(ZOptionKind k, uint64_t zVal) : Kind(k), PageSize(zVal) {}

  ZOption(ZOptionKind k, std::string file) : Kind(k), File(file) {}

  ZOptionKind kind() const { return Kind; }

  void setKind(ZOptionKind pKind) { Kind = pKind; }

  uint64_t pageSize() const { return PageSize; }

  void setPageSize(uint64_t pPageSize) { PageSize = pPageSize; }

  void setFile(std::string file) { File = file; }

  std::string file() const { return File; }

private:
  ZOptionKind Kind;
  uint64_t PageSize;
  std::string File;
};

} // namespace eld

#endif
