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
  enum Kind {
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
  ZOption(Kind k, uint64_t zVal) : m_Kind(k), m_PageSize(zVal) {}

  ZOption(Kind k, std::string file) : m_Kind(k), m_File(file) {}

  Kind kind() const { return m_Kind; }

  void setKind(Kind pKind) { m_Kind = pKind; }

  uint64_t pageSize() const { return m_PageSize; }

  void setPageSize(uint64_t pPageSize) { m_PageSize = pPageSize; }

  void setFile(std::string file) { m_File = file; }

  std::string file() const { return m_File; }

private:
  Kind m_Kind;
  uint64_t m_PageSize;
  std::string m_File;
};

} // namespace eld

#endif
