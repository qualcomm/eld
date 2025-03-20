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

  TargetOptions(const std::string &PTriple);

  ~TargetOptions();

  const llvm::Triple &triple() const { return *Triple; }

  void setTriple(const std::string &PTriple);

  void setTriple(const llvm::Triple &PTriple);

  const std::string &getArch() const { return ArchName; }

  void setArch(const std::string &PArchName);

  const std::string &getTargetCPU() const { return TargetCPU; }

  void setTargetCPU(const std::string &PCpu);

  Endian endian() const { return Endian; }

  void setEndian(Endian PEndian) { Endian = PEndian; }

  bool isLittleEndian() const { return (Little == Endian); }
  bool isBigEndian() const { return (Big == Endian); }

  unsigned int bitclass() const { return BitClass; }

  void setBitClass(unsigned int PBitClass) { BitClass = PBitClass; }

  bool is32Bits() const { return (32 == BitClass); }
  bool is64Bits() const { return (64 == BitClass); }

  bool hasTriple() const { return !!Triple; }

  void addEntrySection(LinkerScript &PScript, llvm::StringRef PPattern);

  WildCardVecTy &getEntrySections();

private:
  std::optional<llvm::Triple> Triple;
  std::string ArchName;
  std::string TargetCPU;
  std::string TargetFS;
  Endian Endian;
  unsigned int BitClass;
  std::vector<WildcardPattern *> EntrySections;
};

} // namespace eld

#endif
