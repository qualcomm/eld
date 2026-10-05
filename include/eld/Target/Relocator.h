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
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include <mutex>

namespace eld {

class GNULDBackend;
class IRBuilder;
class Module;
class InputFile;
struct MergeableConstant;
struct MergeableString;

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
  enum Result { OK, BadReloc, Overflow, BadImm, Unsupport, Unknown };

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

  /// Symbols needing a copy relocation, in the order phase 2 discovered them.
  /// Insertion order is preserved so that the copy relocations, and the
  /// .dynbss sections created for them, are deterministic.
  using CopyRelocs = llvm::SetVector<ResolveInfo *>;

public:
  Relocator(LinkerConfig &pConfig, Module &pModule)
      : m_Config(pConfig), m_Module(pModule) {}

  virtual ~Relocator() = default;

  /// apply - general apply function
  virtual Result applyRelocation(Relocation &pRelocation) = 0;

  virtual bool shouldScanRelocations() const { return true; }

  /// Parallel relocation scanning runs in two phases:
  ///
  /// Phase 1 (scanRelocationParallel) may run concurrently over input files.
  /// It only performs the checks that are common to every backend and records
  /// which relocations need further processing. It creates no GOT/PLT slots
  /// and no dynamic relocations.
  ///
  /// Phase 2 (replayScanRelocations) is serial and visits the recorded
  /// relocations in input order. Slot allocation happens here.
  ///
  /// Prepare for phase 1. Must be called before a parallel scan.
  void initScanRelocations();

  /// Serial single pass scan.
  void scanRelocation(Relocation &Reloc, ELFSection &Section, InputFile &Input,
                      CopyRelocs &CopyRelocSet);

  void scanRelocationParallel(Relocation &Reloc, ELFSection &Section,
                              InputFile &Input, size_t InputIndex,
                              size_t SectIndex, size_t RelocIndex);

  /// Phase 2: Replays the relocations recorded by phase 1 in input
  /// order, serially.
  void replayScanRelocations(CopyRelocs &CopyRelocSet);

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

  virtual void traceMergeConstants(const ELFSection *RelocationSection,
                                   const Relocation *R,
                                   const MergeableConstant *From,
                                   const MergeableConstant *To) const;

  virtual std::pair<Fragment *, uint64_t>
  findFragmentForMergeStr(const ELFSection *RelocationSection,
                          const Relocation *R, MergeStringFragment *F) const;

  virtual bool doMergeStrings(ELFSection *S);
  virtual bool doMergeConstants(ELFSection *S);

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

  bool checkPICRelocSupported(const Relocation &reloc) const;

  bool checkDynamicRelocAllowed(const Relocation &reloc,
                                const ELFSection &section, bool isAbs) const;

  /// Get Symbol Name
  std::string getSymbolName(const ResolveInfo *R) const;

  /// Get section Name
  std::string getSectionName(const ELFSection *S) const;

  // Should symbol names be demangled ?
  bool doDeMangle() const;

  // Get the address for a relocation
  Relocation::Address getSymValue(const Relocation *R);

  std::optional<uint64_t> getFirstTLSSegmentVirtualAddr() const;

  std::optional<uint64_t> getMaxTLSSegmentAlign() const;

  /// Compute and store any offsets that are required for computing
  /// TLS relocations.
  virtual void computeTLSOffsets() {}

private:
  enum ErrType { Undef, Invisible };
  LinkerConfig &m_Config;
  llvm::DenseMap<uint64_t, bool> UndefHits;

protected:
  bool reportNonPICRelocation(const Relocation &reloc) const;

  virtual bool isRelocSupported(const Relocation &Reloc) const { return true; }

  virtual void diagnoseUnsupportedReloc(const Relocation &Reloc,
                                        const ELFSection &Section,
                                        const InputFile &Input) const;

  virtual bool canIssueUndefRef(ResolveInfo *Sym);

  /// Phase 1:  Called for a relocation whose target section is not allocatable,
  /// just before the relocation is dropped.
  virtual void scanNonAllocReloc(Relocation &Reloc, ELFSection &Section) {}

  /// Phase 2: Handle one relocation recorded by phase 1, serially.
  virtual void scanDeferredRelocation(InputFile &Input, Relocation &Reloc,
                                      ELFSection &Section,
                                      CopyRelocs &CopyRelocSet) = 0;

  virtual bool isPICRelocTypeSupported(const Relocation &reloc) const {
    return true;
  }

  virtual bool isDynamicRelocSupported(const Relocation &reloc) const {
    return true;
  }

  Module &m_Module;
  std::mutex m_RelocMutex;
  std::unordered_map<std::string, uint32_t> RelocNameMap;

private:
  bool scanRelocationPrologue(Relocation &Reloc, ELFSection &Section,
                              InputFile &Input);

  struct DeferredInput {
    llvm::SmallVector<llvm::BitVector, 0> RelocationsBySection;
  };
  llvm::SmallVector<DeferredInput, 0> DeferredRelocations;
};

Relocator::Result checkSignedRange(Relocation &Rel, Relocator &R, int64_t Value,
                                   unsigned Bits);

Relocator::Result reportUnsignedOverflow(Relocation &Rel, Relocator &R,
                                         uint64_t Value, unsigned Bits);

} // namespace eld
#endif
