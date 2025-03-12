//===- TargetOptions.h-----------------------------------------------------===//
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
#ifndef ELD_CONFIG_TARGETOPTIONS_H
#define ELD_CONFIG_TARGETOPTIONS_H

#include "eld/Script/WildcardPattern.h"
#include "llvm/TargetParser/Triple.h"
#include <string>

namespace eld {

class LinkerScript;

/** \class TargetOptions
 *  \brief TargetOptions collects the options that dependent on a target
 *  backend.
 */
class TargetOptions {
public:
  enum Endian { Little, Big, Unknown };

public:
  typedef std::vector<WildcardPattern *> WildCardVecTy;
  TargetOptions();

  TargetOptions(const std::string &pTriple);

  ~TargetOptions();

  const llvm::Triple &triple() const { return *m_Triple; }

  void setTriple(const std::string &pTriple);

  void setTriple(const llvm::Triple &pTriple);

  const std::string &getArch() const { return m_ArchName; }

  void setArch(const std::string &pArchName);

  const std::string &getTargetCPU() const { return m_TargetCPU; }

  void setTargetCPU(const std::string &pCPU);

  Endian endian() const { return m_Endian; }

  void setEndian(Endian pEndian) { m_Endian = pEndian; }

  bool isLittleEndian() const { return (Little == m_Endian); }
  bool isBigEndian() const { return (Big == m_Endian); }

  unsigned int bitclass() const { return m_BitClass; }

  void setBitClass(unsigned int pBitClass) { m_BitClass = pBitClass; }

  bool is32Bits() const { return (32 == m_BitClass); }
  bool is64Bits() const { return (64 == m_BitClass); }

  bool hasTriple() const { return !!m_Triple; }

  void addEntrySection(LinkerScript &pScript, llvm::StringRef pPattern);

  WildCardVecTy &getEntrySections();

private:
  std::optional<llvm::Triple> m_Triple;
  std::string m_ArchName;
  std::string m_TargetCPU;
  std::string m_TargetFS;
  Endian m_Endian;
  unsigned int m_BitClass;
  std::vector<WildcardPattern *> m_EntrySections;
};

} // namespace eld

#endif
