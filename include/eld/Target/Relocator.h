//===- Relocator.h---------------------------------------------------------===//
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
#ifndef ELD_TARGET_RELOCATOR_H
#define ELD_TARGET_RELOCATOR_H

#include "eld/Core/Module.h"
#include "eld/Readers/Relocation.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include <mutex>
#include <unordered_set>

namespace eld {

class GNULDBackend;
class IRBuilder;
class Module;
class InputFile;

/** \class Relocator
 *  \brief Relocator provides the interface of performing relocations
 */
class Relocator {
public:
  typedef Relocation::Type Type;
  typedef Relocation::Address Address;
  typedef Relocation::DWord DWord;
  typedef Relocation::SWord SWord;
  typedef Relocation::Size Size;

public:
  enum Result { OK, BadReloc, Overflow, Unsupport, Unknown };

  /** \enum ReservedEntryType
   *  \brief The reserved entry type of reserved space in ResolveInfo.
   *
   *  This is used for sacnRelocation to record what kinds of entries are
   *  reserved for this resolved symbol. In Hexagon, there are three kinds
   *  of entries, GOT, PLT, and dynamic relocation.
   *
   *  bit:  3     2     1     0
   *   |    | PLT | GOT | Rel |
   *
   *  value    Name         - Description
   *
   *  0000     None         - no reserved entry
   *  0001     ReserveRel   - reserve an dynamic relocation entry
   *  0010     ReserveGOT   - reserve an GOT entry
   *  0100     ReservePLT   - reserve an PLT entry and the corresponding GOT,
   *
   */
  enum ReservedEntryType {
    None = 0,
    ReserveRel = 1,
    ReserveGOT = 2,
    ReservePLT = 4,
  };

  typedef std::unordered_set<ResolveInfo *> CopyRelocs;

public:
  Relocator(LinkerConfig &pConfig, Module &pModule)
      : m_Config(pConfig), m_Module(pModule) {}

  virtual ~Relocator() = 0;

  /// apply - general apply function
  virtual Result applyRelocation(Relocation &pRelocation) = 0;

  virtual void scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                              ELFSection &pSection, InputFile &pInput,
                              CopyRelocs &CopyRelocs) = 0;

  // Issue an undefined reference error if the symbol is a magic section symbol.
  void issueUndefRefForMagicSymbol(const Relocation &pReloc);

  virtual void issueUndefRef(const Relocation &pReloc, InputFile &pInput,
                             ELFSection *pSection = nullptr);

  virtual void issueInvisibleRef(Relocation &pReloc, InputFile &pInput);

  virtual void partialScanRelocation(Relocation &pReloc,
                                     const ELFSection &pSection);

  /// Merge string relocations are modified to point directly to the string so
  /// an addend is not required.
  virtual void adjustAddend(Relocation *R) const { R->setAddend(0); }

  virtual uint32_t getAddend(const Relocation *R) const { return R->addend(); }

  virtual void traceMergeStrings(const ELFSection *RelocationSection,
                                 const Relocation *R,
                                 const MergeableString *From,
                                 const MergeableString *To) const;

  virtual std::pair<Fragment *, uint64_t>
  findFragmentForMergeStr(const ELFSection *RelocationSection,
                          const Relocation *R, MergeStringFragment *F) const;

  virtual bool doMergeStrings(ELFSection *S);

  // ------ observers -----//
  virtual GNULDBackend &getTarget() = 0;

  virtual const GNULDBackend &getTarget() const = 0;

  /// getName - get the name of a relocation
  virtual const char *getName(Type pType) const = 0;

  /// getSize - get the size of a relocation in bits
  virtual Size getSize(Type pType) const = 0;

  virtual uint32_t relocType() const { return llvm::ELF::SHT_RELA; }

  virtual void checkCrossReferences(Relocation &pReloc, InputFile &pInput,
                                    ELFSection &pReferredSect);

  LinkerConfig &config() const { return m_Config; }
  Module &module() const { return m_Module; }

  virtual uint32_t getNumRelocs() const = 0;

  uint32_t getRelocType(std::string Name);

  /// Get Symbol Name
  std::string getSymbolName(const ResolveInfo *R) const;

  /// Get section Name
  std::string getSectionName(const ELFSection *S) const;

  // Should symbol names be demangled ?
  bool doDeMangle() const;

  // Get the address for a relocation
  Relocation::Address getSymValue(Relocation *R);

private:
  enum ErrType { Undef, Invisible };
  LinkerConfig &m_Config;
  // DenseSet is behaving crazily failing in looking up a bucket, fails once in
  // 100000 runs, lets use std::unordered_map.
  std::unordered_map<uint64_t, bool> m_UndefHits;

protected:
  Module &m_Module;
  std::mutex m_RelocMutex;
  std::unordered_map<std::string, uint32_t> RelocNameMap;
};

} // namespace eld

#endif
