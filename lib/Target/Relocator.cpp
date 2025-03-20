//===- Relocator.cpp-------------------------------------------------------===//
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
#include "eld/Target/Relocator.h"
#include "eld/Config/Config.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/MergeStringFragment.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/SymbolInfo.h"
#include "eld/Target/ELFFileFormat.h"
#include <sstream>

using namespace eld;

//===----------------------------------------------------------------------===//
// Relocator
//===----------------------------------------------------------------------===//
Relocator::~Relocator() {}

void Relocator::partialScanRelocation(Relocation &pReloc,
                                      const ELFSection &pSection) {
  // if we meet a section symbol
  if (pReloc.symInfo()->type() == ResolveInfo::Section) {
    LDSymbol *input_sym = pReloc.symInfo()->outSymbol();

    // 1. update the relocation target offset
    assert(input_sym->hasFragRef());
    uint64_t offset = input_sym->fragRef()->getOutputOffset(m_Module);
    pReloc.target() += offset;

    // 2. get output section symbol
    // get the output ELFSection which the symbol defined in
    ELFSection *out_sect = input_sym->fragRef()->getOutputELFSection();
    ResolveInfo *sym_info = m_Module.getSectionSymbol(out_sect);
    // set relocation target symbol to the output section symbol's resolveInfo
    pReloc.setSymInfo(sym_info);
  }
}

void Relocator::traceMergeStrings(const ELFSection *RelocationSection,
                                  const Relocation *R,
                                  const MergeableString *From,
                                  const MergeableString *To) const {
  ELFSection *OutputSection =
      From->Fragment->getOwningSection()->getOutputELFSection();
  std::string OutputSectionName = From->Fragment->getOwningSection()
                                      ->getOutputELFSection()
                                      ->getDecoratedName(m_Config.options());
  if (!m_Config.options().shouldTraceMergeStrSection(OutputSection))
    return;
  std::string SymName = R->symInfo()->name();
  uint32_t Addend = R->addend();
  std::string Section = RelocationSection->getDecoratedName(m_Config.options());
  std::string File =
      RelocationSection->getInputFile()->getInput()->decoratedPath();
  uint32_t OldOffset = From->InputOffset;
  std::string OldSection =
      From->Fragment->getOwningSection()->getDecoratedName(m_Config.options());
  std::string OldFile = From->Fragment->getOwningSection()
                            ->getInputFile()
                            ->getInput()
                            ->decoratedPath();
  uint32_t NewOffset = To->InputOffset;
  std::string NewSection =
      To->Fragment->getOwningSection()->getDecoratedName(m_Config.options());
  std::string NewFile = To->Fragment->getOwningSection()
                            ->getInputFile()
                            ->getInput()
                            ->decoratedPath();

  m_Config.raise(Diag::modifying_mergestr_reloc)
      << SymName << Addend << Section << File << OldOffset << OldSection
      << OldFile << NewOffset << NewSection << NewFile;
}

std::pair<Fragment *, uint64_t>
Relocator::findFragmentForMergeStr(const ELFSection *RelocationSection,
                                   const Relocation *R,
                                   MergeStringFragment *F) const {
  uint32_t Addend = getAddend(R);
  MergeableString *String = F->findString(Addend);
  uint32_t OffsetInString = Addend - String->InputOffset;

  if (!String)
    return {nullptr, 0};

  OutputSectionEntry *OutputSection = F->getOwningSection()->getOutputSection();
  bool GlobalMerge = m_Config.options().shouldGlobalStringMerge();
  if (MergeableString *DeDuped =
          (!F->getOwningSection()->isAlloc() && GlobalMerge)
              ? m_Module.getMergedNonAllocString(String)
              : OutputSection->getMergedString(String)) {
    if (m_Config.getPrinter()->isVerbose() ||
        m_Config.getPrinter()->traceMergeStrings())
      traceMergeStrings(RelocationSection, R, String, DeDuped);
    String = DeDuped;
  }

  return {String->Fragment, String->InputOffset + OffsetInString};
}

bool Relocator::doMergeStrings(ELFSection *S) {
  auto DoMergeStrReloc = [&](Relocation *R) -> void {
    if (!R->isMergeKind() || !R->symInfo()->isSection() ||
        getTarget().maySkipRelocProcessing(R))
      return;
    FragmentRef *OldTarget = R->targetFragRef()
                                 ? R->targetFragRef()
                                 : R->symInfo()->outSymbol()->fragRef();

    auto *MSF = llvm::cast<MergeStringFragment>(OldTarget->frag());
    auto [Frag, Offset] = findFragmentForMergeStr(S, R, MSF);
    if (!Frag)
      return;
    adjustAddend(R);
    /// Replace the target to point to the string directly.
    R->modifyRelocationFragmentRef(make<FragmentRef>(*Frag, Offset));
  };

  for (Relocation *R : S->getLink()->getRelocations())
    DoMergeStrReloc(R);
  return true;
}

std::string Relocator::getSymbolName(const ResolveInfo *R) const {
  return m_Module.getNamePool().getDecoratedName(
      R, m_Config.options().shouldDemangle());
}

std::string Relocator::getSectionName(const ELFSection *S) const {
  if (!S)
    return "";
  if (S->isRelocationKind())
    S = S->getLink();
  return S->getDecoratedName(m_Config.options());
}

void Relocator::issueUndefRefForMagicSymbol(const Relocation &Reloc) {
  ResolveInfo *R = Reloc.symInfo();
  if (R && R->isUndef() && !R->isDyn() && !R->isWeak() && !R->isNull() &&
      getTarget().isSectionMagicSymbol(R->getName())) {
    issueUndefRef(Reloc, *R->resolvedOrigin(), nullptr);
  }
}

void Relocator::issueInvisibleRef(Relocation &pReloc, InputFile &pInput) {
  ResolveInfo *rsym = pReloc.symInfo();
  if (m_Config.options().warnOnce()) {
    uint64_t hash = llvm::hash_combine(std::string(rsym->name()), Invisible);
    auto sym = m_UndefHits.find(hash);
    if (sym != m_UndefHits.end() && sym->second == Invisible)
      return;
    m_UndefHits.insert(std::make_pair(hash, true));
  }

  llvm::StringRef link_type = m_Config.codeGenType() == LinkerConfig::DynObj
                                  ? "a shared object"
                                  : "an executable";
  config().raise(Diag::undef_sym_visibility)
      << pInput.getInput()->decoratedPath() << getName(pReloc.type())
      << getSymbolName(rsym) << rsym->getVisibilityString() << link_type;
}

void Relocator::checkCrossReferences(Relocation &pReloc, InputFile &pInput,
                                     ELFSection &pReferredSect) {
  if (pInput.isInternal())
    return;
  auto no_cross_refs = m_Module.getNonRefSections();

  OutputSectionEntry *src_sect = pReferredSect.getOutputSection();
  if (!src_sect)
    return;
  auto src_in_list = no_cross_refs.find(src_sect->name().str());
  if (src_in_list == no_cross_refs.end())
    return;

  LDSymbol *outSym = pReloc.symInfo()->outSymbol();
  ELFSection *target_sect = nullptr;
  if (outSym)
    target_sect = pReloc.outputSection();
  if (!target_sect)
    return;
  auto target_in_list = no_cross_refs.find(target_sect->name().str());
  if (target_in_list == no_cross_refs.end() ||
      target_in_list->second != src_in_list->second)
    return;

  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);

  std::stringstream ss;
  ss << "0x" << std::hex << pReloc.targetRef()->offset();
  std::string caller_sym_pos_hex(ss.str());

  std::string caller_file_name = pInput.getInput()->decoratedPath();
  InputFile *defined_file = outSym->resolveInfo()->resolvedOrigin();
  std::string callee_file_name = defined_file->getInput()->decoratedPath();
  config().raise(Diag::prohibited_cross_ref)
      << getSymbolName(pReloc.symInfo()) << caller_file_name
      << getSectionName(&pReferredSect) << caller_sym_pos_hex
      << getSectionName(src_sect->getSection()) << getSectionName(target_sect)
      << callee_file_name;
  if (!config().getDiagEngine()->diagnose())
    m_Module.setFailure(true);
}

void Relocator::issueUndefRef(const Relocation &pReloc, InputFile &pInput,
                              ELFSection *pSection) {

  std::string reloc_sym(pReloc.symInfo()->name());
  FragmentRef::Offset undef_sym_pos = pReloc.targetRef()->offset();

  if (m_Config.options().warnOnce()) {
    uint64_t hash = llvm::hash_combine(reloc_sym, Undef);
    auto sym = m_UndefHits.find(hash);
    if (sym != m_UndefHits.end() && sym->second == true)
      return;
    m_UndefHits.insert(std::make_pair(hash, true));
  }

  if (!config().getDiagEngine()->diagnose())
    m_Module.setFailure(true);

  std::stringstream ss;
  ss << "0x" << std::hex << undef_sym_pos;
  std::string undef_sym_pos_hex(ss.str());

  ResolveInfo *caller_file = nullptr;
  ResolveInfo *caller_func = nullptr;

  if (pReloc.symInfo()->type() == ResolveInfo::Function) {
    ELFObjectFile *ObjFile = llvm::dyn_cast<eld::ELFObjectFile>(&pInput);
    for (auto &sym : ObjFile->getSymbols()) {
      if (sym->resolveInfo()->isFile())
        caller_file = sym->resolveInfo();

      if (sym->resolveInfo()->isFunc() && sym->value() <= undef_sym_pos &&
          sym->value() + sym->size() > undef_sym_pos)
        caller_func = sym->resolveInfo();

      if (caller_file && caller_func)
        break;
    }
  }

  if (!caller_file || !caller_func) {
    config().raise(Diag::undefined_reference)
        << getSymbolName(pReloc.symInfo()) << pInput.getInput()->decoratedPath()
        << getSectionName(pSection) << undef_sym_pos_hex;
    return;
  }

  config().raise(Diag::undefined_reference_text)
      << getSymbolName(pReloc.symInfo()) << pInput.getInput()->decoratedPath()
      << getSymbolName(caller_file) << getSymbolName(caller_func);
}

uint32_t Relocator::getRelocType(std::string RelocName) {
  if (!RelocNameMap.size()) {
    for (uint32_t I = 0; I < getNumRelocs(); ++I) {
      std::string Name = getName(I);
      if (Name.empty())
        continue;
      RelocNameMap[Name] = I;
    }
  }
  if (RelocNameMap.find(RelocName) == RelocNameMap.end())
    return UINT32_MAX;
  return RelocNameMap[RelocName];
}

bool Relocator::doDeMangle() const {
  return m_Config.options().shouldDemangle();
}

Relocation::Address Relocator::getSymValue(Relocation *R) {
  if (R->symInfo() && R->symInfo()->isThreadLocal())
    return getTarget().finalizeTLSSymbol(R->symInfo()->outSymbol());
  return R->symValue(m_Module);
}
