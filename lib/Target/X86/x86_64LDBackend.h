//===- x86_64LDBackend.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef X86_64_LDBACKEND_H
#define X86_64_LDBACKEND_H

#include "eld/Config/LinkerConfig.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"
#include "x86_64PLT.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/ELF.h"
#include <unordered_set>

namespace eld {

class LinkerConfig;
class x86_64Info;

//===----------------------------------------------------------------------===//
/// x86_64LDBackend - linker backend of x86_64 target of GNU ELF format
///
class x86_64LDBackend : public GNULDBackend {
public:
  x86_64LDBackend(Module &pModule, x86_64Info *pInfo);

  ~x86_64LDBackend();

  void initializeAttributes() override;

  /// initRelocator - create and initialize Relocator.
  bool initRelocator() override;

  /// getRelocator - return relocator.
  Relocator *getRelocator() const override;

  void initTargetSections(ObjectBuilder &pBuilder) override;

  /// Create dynamic input sections in an input file.
  void initDynamicSections(ELFObjectFile &) override;

  void initTargetSymbols() override;

  bool initBRIslandFactory() override;

  bool initStubFactory() override;

  bool shouldIgnoreRelocSync(Relocation *Reloc) const override;

  eld::Expected<void> postProcessing(llvm::FileOutputBuffer &pOutput) override;

  /// getTargetSectionOrder - compute the layout order of target section
  unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const override;

  /// finalizeTargetSymbols - finalize the symbol value
  bool finalizeTargetSymbols() override;

  uint64_t getValueForDiscardedRelocations(const Relocation *R) const override;

  void reserveTargetDynamicEntries() override;
  void applyTargetDynamicEntries() override;

  void doCreateProgramHdrs() override { return; }

  Stub *getBranchIslandStub(Relocation *pReloc,
                            int64_t pTargetValue) const override {
    return nullptr;
  }

  Relocation::Type getCopyRelType() const override;

  // ---  GOT Support ------
  x86_64GOT *createGOT(GOT::GOTType T, ELFObjectFile *Obj, ResolveInfo *sym);

  void recordGOT(ResolveInfo *, x86_64GOT *);

  void recordGOTPLT(ResolveInfo *, x86_64GOT *);

  x86_64GOT *findEntryInGOT(ResolveInfo *) const;

  // ---  PLT Support ------
  // Create a PLT entry for a symbol. If isIRelative is true, emit an
  // IRELATIVE relocation targeting the GOTPLT slot and build a PLT entry
  // suitable for eager binding (no PLT0 back-edge).
  x86_64PLT *createPLT(ELFObjectFile *Obj, ResolveInfo *sym,
                       bool isIRelative = false);

  void recordPLT(ResolveInfo *, x86_64PLT *);

  x86_64PLT *findEntryInPLT(ResolveInfo *) const;

  std::size_t PLTEntriesCount() const override { return m_PLTMap.size(); }

  std::size_t GOTEntriesCount() const override { return m_GOTMap.size(); }

  void setDefaultConfigs() override;

  void doPreLayout() override;

  void sortRelocation(ELFSection &pSection) override;

  /// Returns true if the relocation is eligible for GOTPCRELX relaxation.
  /// Checks: type R_X86_64_GOTPCRELX, addend == -4, non-preemptible,
  /// not IFUNC, and opcode is MOV/CALL/JMP (read directly from the input
  /// fragment). Relaxation is disabled for partial links.
  bool isGOTPCRELXRelaxable(const Relocation *reloc) const;

  /// Records a relocation that the scan phase has decided to relax, so that
  /// postProcessing can iterate only these candidates instead of re-walking
  /// every relocation. Thread-safe: called from the parallel scan under the
  /// relocator mutex.
  void recordGOTPCRELXRelaxCandidate(Relocation *reloc) {
    m_GOTPCRELXRelaxCandidates.insert(reloc);
  }

  /// Returns true if this relocation was recorded as a relaxation candidate
  /// during the scan phase. Used by shouldIgnoreRelocSync and the apply-path
  /// guard to avoid re-deriving relaxability.
  bool isGOTPCRELXRelaxCandidate(Relocation *reloc) const {
    return m_GOTPCRELXRelaxCandidates.count(reloc) != 0;
  }

  /// Records an R_X86_64_TLSLD relocation as an LD→LE relaxation candidate.
  void recordTLSLDRelaxCandidate(Relocation *reloc) {
    const Fragment *frag = reloc->targetRef()->frag();
    uint32_t offset = static_cast<uint32_t>(reloc->targetRef()->offset());
    m_TLSLDRelaxCandidates.insert(reloc);
    m_TLSLDFragOffsets.insert({frag, offset});
    m_TLSLDByFragment[frag].push_back(offset);
  }

  /// Records an R_X86_64_DTPOFF32/64 relocation that belongs to a relaxed
  /// LD sequence. After LD→LE, %rax = %fs:0 (TP), so the DTPOFF value must
  /// use the TP-relative formula (S + A - TLSTemplateSize) instead of S + A.
  void recordTLSLDRelaxDTPOFF(Relocation *reloc) {
    m_TLSLDRelaxDTPOFF.insert(reloc);
  }

  bool isTLSLDRelaxDTPOFF(const Relocation *reloc) const {
    return m_TLSLDRelaxDTPOFF.count(const_cast<Relocation *>(reloc)) != 0;
  }

  /// Returns true if this DTPOFF32/64 reloc is in the same fragment as a
  /// recorded TLSLD reloc at a lower offset, meaning its base (%rax) was
  /// set by the LD→LE rewrite to %fs:0 (TP) rather than the module base.
  bool isDTPOFFInRelaxedLDSequence(const Relocation *reloc) const {
    if (!config().isBuildingExecutable())
      return false;
    const Fragment *frag = reloc->targetRef()->frag();
    uint32_t off = reloc->targetRef()->offset();
    auto it = m_TLSLDByFragment.find(frag);
    if (it == m_TLSLDByFragment.end())
      return false;
    // O(k) where k = TLSLD sites in this fragment (typically 1), not O(n)
    // over all TLSLD sites across all input files.
    for (uint32_t tlsld_off : it->second)
      if (tlsld_off < off)
        return true;
    return false;
  }

  /// Returns true if this R_X86_64_TLSLD was marked for LD→LE relaxation.
  bool isTLSLDLERelaxCandidate(Relocation *reloc) const {
    return m_TLSLDRelaxCandidates.count(reloc) != 0;
  }

  /// Records a TLSLD relocation if it meets all LD→LE eligibility conditions.
  /// Called from both scanLocalReloc and scanGlobalReloc under the mutex.
  bool recordTLSLDLERelaxCandidate(Relocation *reloc);

  /// Returns true if this relocation is the paired call in a LD→LE relaxed
  /// sequence. Identified positionally: two offsets are valid depending on
  /// the call variant in the same fragment:
  ///   +5: direct call  (e8 <rel32>)        — PLT32, PC32
  ///   +6: indirect call (ff 15 <rel32>)    — GOTPCRELX, GOTPCREL
  /// Works for any companion reloc type — no symbol-name matching needed.
  bool isTLSLDCompanion(const Relocation *reloc) const {
    uint32_t offset = reloc->targetRef()->offset();
    const Fragment *frag = reloc->targetRef()->frag();
    return (offset >= 5 && m_TLSLDFragOffsets.count({frag, offset - 5}) != 0) ||
           (offset >= 6 && m_TLSLDFragOffsets.count({frag, offset - 6}) != 0);
  }

  /// Returns true if this relocation is part of a LD→LE relaxed sequence:
  /// either the TLSLD itself or its paired companion call reloc.
  bool isTLSLDRelaxCandidate(Relocation *reloc) const {
    return isTLSLDLERelaxCandidate(reloc) || isTLSLDCompanion(reloc);
  }

  DynRelocType getDynRelocType(const Relocation *X) const override {
    if (X->type() == llvm::ELF::R_X86_64_GLOB_DAT)
      return DynRelocType::GLOB_DAT;
    if (X->type() == llvm::ELF::R_X86_64_JUMP_SLOT)
      return DynRelocType::JMP_SLOT;
    if (X->type() == llvm::ELF::R_X86_64_RELATIVE ||
        X->type() == llvm::ELF::R_X86_64_IRELATIVE)
      return DynRelocType::RELATIVE;
    if (X->type() == llvm::ELF::R_X86_64_DTPMOD64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPMOD_LOCAL;
      return DynRelocType::DTPMOD_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_X86_64_DTPOFF64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::DTPREL_LOCAL;
      return DynRelocType::DTPREL_GLOBAL;
    }
    if (X->type() == llvm::ELF::R_X86_64_TPOFF64) {
      if (X->symInfo() && X->symInfo()->binding() == ResolveInfo::Local)
        return DynRelocType::TPREL_LOCAL;
      return DynRelocType::TPREL_GLOBAL;
    }
    return DynRelocType::DEFAULT;
  }

  bool hasSymInfo(const Relocation *X) const override {
    if (!X->symInfo()) {
      return false;
    }
    if (X->type() == llvm::ELF::R_X86_64_RELATIVE ||
        X->type() == llvm::ELF::R_X86_64_IRELATIVE)
      return false; // RELATIVE and IRELATIVE relocations do not encode a symbol
    if (X->symInfo()->binding() == ResolveInfo::Local)
      return false; // locals are not in .dynsym
    return true;
  }

private:
  /// getRelEntrySize - the size in BYTE of rela type relocation
  size_t getRelEntrySize() override { return 0; }

  /// getRelaEntrySize - the size in BYTE of rela type relocation
  size_t getRelaEntrySize() override { return 24; }

  uint64_t maxBranchOffset() override { return 0; }

  enum class GOTPCRELXOpcode { MOV, CALL, JMP, Unknown };
  GOTPCRELXOpcode getGOTPCRELXOpcode(uint8_t op, uint8_t modrm) const;

  /// Rewrites the GOT-indirect instruction referenced by a single relaxation
  /// candidate into its PC-relative form, patching the output buffer.
  /// Returns an error (and does not patch) if the displacement overflows a
  /// signed 32-bit field.
  eld::Expected<void> relaxGOTPCRELXReloc(Relocation *reloc, uint8_t *buf);

  /// Rewrites the TLSLD leaq+call sequence to the LD→LE fixed 12-byte form
  /// (3×data16 + movq %fs:0, %rax) for static builds.
  eld::Expected<void> relaxTLSLDReloc(Relocation *reloc, uint8_t *buf);

  /// Iterates the cached relaxation candidates and rewrites each one.
  eld::Expected<void> doRelax(llvm::FileOutputBuffer &pOutput);

private:
  Relocator *m_pRelocator;

  LDSymbol *m_pEndOfImage;
  // Tracks .rela.plt entry index
  // m_RelaPLTIndex starts at 0 for the first function PLT entry
  uint32_t m_RelaPLTIndex = 0;

  llvm::DenseMap<ResolveInfo *, x86_64GOT *> m_GOTMap;
  llvm::DenseMap<ResolveInfo *, x86_64GOT *> m_GOTPLTMap;
  llvm::DenseMap<ResolveInfo *, x86_64PLT *> m_PLTMap;
  /// Relocations selected for GOTPCRELX relaxation during the scan phase.
  /// Populated under the relocator mutex; consumed single-threaded in
  /// postProcessing. unordered_set for O(1) membership checks in
  /// shouldIgnoreRelocSync and the apply-path guard.
  std::unordered_set<Relocation *> m_GOTPCRELXRelaxCandidates;

  /// R_X86_64_TLSLD relocations selected for LD→LE relaxation in static
  /// builds. Populated under the relocator mutex in scan; consumed
  /// single-threaded in postProcessing.
  std::unordered_set<Relocation *> m_TLSLDRelaxCandidates;

  /// (fragment, offset) pairs of every recorded TLSLD reloc, used by
  /// isTLSLDCompanion for exact-offset companion detection.
  llvm::DenseSet<std::pair<const Fragment *, uint32_t>> m_TLSLDFragOffsets;

  /// Per-fragment list of TLSLD offsets, used by isDTPOFFInRelaxedLDSequence
  /// to tag DTPOFF relocs that follow a relaxed TLSLD in the same fragment.
  llvm::DenseMap<const Fragment *, llvm::SmallVector<uint32_t, 2>>
      m_TLSLDByFragment;

  /// DTPOFF32/64 relocs that belong to a relaxed LD sequence.
  /// Only these need the TP-relative formula; all other DTPOFF are S+A.
  std::unordered_set<Relocation *> m_TLSLDRelaxDTPOFF;
};
} // namespace eld

#endif
