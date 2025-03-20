//===- Relocation.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/Relocation.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/Relocator.h"
#include "llvm/Support/ManagedStatic.h"

namespace eld {
class DiagnosticEngine;
}

using namespace eld;

llvm::DenseMap<const Relocation *, FragmentRef *> Relocation::m_RelocFragMap;

//===----------------------------------------------------------------------===//
// Relocation Factory Methods
//===----------------------------------------------------------------------===//

/// Create - produce an empty relocation entry
Relocation *Relocation::Create() { return make<Relocation>(0, nullptr, 0, 0); }

/// Create - produce a relocation entry
/// @param pType    [in] the type of the relocation entry
/// @param pFragRef [in] the place to apply the relocation
/// @param pAddend  [in] the addend of the relocation entry
Relocation *Relocation::Create(Type pType, Size pSize, FragmentRef *pFragRef,
                               Address pAddend) {
  DWord targetData = 0;
  if (pSize != 0)
    pFragRef->memcpy(&targetData, pSize / 8);
  return make<Relocation>(pType, pFragRef, pAddend, targetData);
}

/// Destroy - destroy a relocation entry
void Relocation::Destroy(Relocation *&pRelocation) { pRelocation = nullptr; }

//===----------------------------------------------------------------------===//
// Relocation
//===----------------------------------------------------------------------===//
Relocation::Relocation()
    : m_pSymInfo(nullptr), m_TargetAddress(0x0), m_Addend(0x0), m_TargetData(0),
      m_Type(0) {}

Relocation::Relocation(Relocation::Type pType, FragmentRef *pTargetRef,
                       Relocation::Address pAddend,
                       Relocation::DWord pTargetData)
    : m_pSymInfo(nullptr), m_TargetAddress(pTargetRef), m_Addend(pAddend),
      m_TargetData(pTargetData), m_Type(pType) {}

Relocation::Relocation(const Relocator *pRelocator, Relocation::Type pType,
                       FragmentRef *pTargetRef, Relocation::Address pAddend)
    : m_pSymInfo(nullptr), m_TargetAddress(pTargetRef), m_Addend(pAddend),
      m_TargetData(0), m_Type(pType) {
  Size relocSize = pRelocator->getSize(pType);
  if (relocSize != 0)
    pTargetRef->memcpy(&m_TargetData, relocSize);
}

Relocation::~Relocation() {}

Relocation::Address Relocation::place(Module &M) const {
  Address sect_addr = m_TargetAddress->getOutputELFSection()->addr();
  return sect_addr + m_TargetAddress->getOutputOffset(M);
}

Relocation::Address Relocation::symValue(Module &M) const {
  const ELFSection *section = nullptr;
  const FragmentRef *fragRef = nullptr;
  ResolveInfo *info = m_pSymInfo;

  if (!info || info->outSymbol()->hasFragRef()) {
    FragmentRef *pTargetFragRef = targetFragRef();
    if (pTargetFragRef)
      fragRef = pTargetFragRef;
    else
      fragRef = info->outSymbol()->fragRef();
    section = fragRef->getOutputELFSection();
  }

  // If symbol type is TLS, then only offset
  if (fragRef && info && info->type() == ResolveInfo::ThreadLocal)
    ASSERT(0, llvm::Twine("target relocator needs to be called for symbol " +
                          info->getName())
                  .str());

  bool isAllocSection = section ? section->isAlloc() : false;

  // If allocatable section, value => (address + offset)
  if (isAllocSection)
    return section->addr() + fragRef->getOutputOffset(M);

  // If non allocatable section, value => (offset)
  if (section && !isAllocSection)
    return fragRef->getOutputOffset(M);

  return info->outSymbol()->value();
}

Relocation::Address Relocation::symOffset(Module &M) const {
  const ELFSection *section = nullptr;
  const FragmentRef *fragRef = nullptr;
  ResolveInfo *info = symInfo();

  if (info->outSymbol()->hasFragRef()) {
    FragmentRef *pTargetFragRef = targetFragRef();
    if (pTargetFragRef)
      fragRef = pTargetFragRef;
    else
      fragRef = info->outSymbol()->fragRef();
    section = fragRef->getOutputELFSection();
  }

  bool isAllocSection = section ? section->isAlloc() : false;

  // If allocatable section, value => (address + offset)
  if (isAllocSection)
    return (fragRef->getOutputOffset(M) + addend());

  // If non allocatable section, value => (offset)
  if (section && !isAllocSection)
    return fragRef->getOutputOffset(M);

  return info->outSymbol()->value();
}

ELFSection *Relocation::targetSection() const {
  ResolveInfo *info = symInfo();
  if (!info || !info->outSymbol() || !info->outSymbol()->fragRef())
    return nullptr;
  FragmentRef *fragRef = targetFragRef();
  if (!fragRef)
    fragRef = info->outSymbol()->fragRef();
  Fragment *F = fragRef->frag();
  if (!F)
    return nullptr;
  return F->getOwningSection();
}

ELFSection *Relocation::outputSection() const {
  ResolveInfo *info = symInfo();
  if (info->outSymbol()->hasFragRef())
    return (info->outSymbol()->fragRef()->getOutputELFSection());
  return nullptr;
}

std::string Relocation::getSourcePath(const GeneralOptions &options) const {
  return getFragmentPath(nullptr, targetRef()->frag(), options);
}

std::string Relocation::getTargetPath(const GeneralOptions &options) const {
  return getFragmentPath(nullptr, symInfo()->outSymbol()->fragRef()->frag(),
                         options);
}

ELFSegment *Relocation::getTargetSegment() const {
  Fragment *frag = symInfo()->outSymbol()->fragRef()->frag();
  ELFSection *S = frag->getOutputELFSection();
  if (!S)
    return nullptr;
  return S->getOutputSection()->getLoadSegment();
}

std::string Relocation::getFragmentPath(ResolveInfo *symInfo, Fragment *frag,
                                        const GeneralOptions &options) {
  if (symInfo && symInfo->isCommon())
    return llvm::Twine(symInfo->resolvedOrigin()->getInput()->decoratedPath() +
                       llvm::Twine("[COMMON]"))
        .str();
  // FIXME: Need to handle true linker internal sections
  if (frag && frag->getOwningSection()) {
    std::string decoratedPath;
    ELFSection *S = frag->getOwningSection();
    // Use old file name if available. It is required to show the actual
    // originating file name in case of LTO inputs.
    if (S->getOldInputFile())
      decoratedPath = S->getOldInputFile()->getInput()->decoratedPath(
          /*showAbsolute=*/false);
    else if (S->getInputFile())
      decoratedPath =
          S->getInputFile()->getInput()->decoratedPath(/*showAbsolute=*/false);
    if (!decoratedPath.empty())
      return llvm::Twine(decoratedPath + llvm::Twine("[") +
                         getSectionName(frag->getOwningSection(), options) +
                         llvm::Twine("]"))
          .str();
  }
  if (symInfo && (symInfo != ResolveInfo::null()))
    return symInfo->resolvedOrigin()->getInput()->decoratedPath(0);
  return "(Not Applicable)";
}

bool Relocation::issueOverflow(Relocator &pRelocator) {
  DiagnosticEngine *DiagEngine = pRelocator.config().getDiagEngine();
  const GeneralOptions &options = pRelocator.config().options();
  DiagEngine->raise(Diag::result_overflow_moreinfo)
      << pRelocator.getName(type())
      << getSymbolName(symInfo(), pRelocator.doDeMangle())
      << getFragmentPath(nullptr, targetRef()->frag(), options)
      << getFragmentPath(symInfo(),
                         symInfo()->outSymbol()->hasFragRef()
                             ? symInfo()->outSymbol()->fragRef()->frag()
                             : nullptr,
                         options);
  return false;
}

bool Relocation::apply(Relocator &pRelocator) {
  // Issue undefined references for section magic symbols here
  pRelocator.issueUndefRefForMagicSymbol(*this);

  Module &module = pRelocator.module();
  if (symInfo() && module.getPrinter()->traceSym() &&
      pRelocator.config().options().traceSymbol(*symInfo())) {
    pRelocator.config().raise(Diag::apply_relocation)
        << symInfo()->name()
        << llvm::utohexstr(place(module),
                           /*LowerCase*/ true);
  }

  Relocator::Result result = pRelocator.applyRelocation(*this);
  DiagnosticEngine *DiagEngine = pRelocator.config().getDiagEngine();
  const GeneralOptions &options = pRelocator.config().options();
  switch (result) {
  case Relocator::OK: {
    // do nothing
    return true;
  }

  case Relocator::Overflow:
    return issueOverflow(pRelocator);

  case Relocator::BadReloc: {
    DiagEngine->raise(Diag::result_badreloc_moreinfo)
        << pRelocator.getName(type())
        << getSymbolName(symInfo(), pRelocator.doDeMangle())
        << getFragmentPath(nullptr, targetRef()->frag(), options)
        << getFragmentPath(symInfo(),
                           symInfo()->outSymbol()->hasFragRef()
                               ? symInfo()->outSymbol()->fragRef()->frag()
                               : nullptr,
                           options);
    return false;
  }
  case Relocator::Unsupport: {
    DiagEngine->raise(Diag::unsupported_relocation_moreinfo)
        << pRelocator.getName(type())
        << getSymbolName(symInfo(), pRelocator.doDeMangle())
        << getFragmentPath(nullptr, targetRef()->frag(), options)
        << getFragmentPath(symInfo(),
                           symInfo()->outSymbol()->hasFragRef()
                               ? symInfo()->outSymbol()->fragRef()->frag()
                               : nullptr,
                           options);
    return false;
  }
  case Relocator::Unknown: {
    DiagEngine->raise(Diag::unknown_relocation_moreinfo)
        << pRelocator.getName(type())
        << getSymbolName(symInfo(), pRelocator.doDeMangle())
        << getFragmentPath(nullptr, targetRef()->frag(), options)
        << getFragmentPath(symInfo(),
                           symInfo()->outSymbol()->hasFragRef()
                               ? symInfo()->outSymbol()->fragRef()->frag()
                               : nullptr,
                           options);
    return false;
  }
  } // end of switch
  return false;
}

void Relocation::setType(Type pType) { m_Type = pType; }

void Relocation::setAddend(Address pAddend) { m_Addend = pAddend; }

void Relocation::setSymInfo(ResolveInfo *pSym) { m_pSymInfo = pSym; }

void Relocation::setTargetData(DWord pTargetData) {
  m_TargetData = pTargetData;
}

Relocation::Size Relocation::size(Relocator &pRelocator) const {
  return pRelocator.getSize(m_Type);
}

// Pass diagnostic engine here as argument.
void Relocation::updateAddend(Module &M) {
  ResolveInfo *info = m_pSymInfo;
  // Update value keep in addend if we meet a section symbol
  if (info->type() == ResolveInfo::Section) {
    uint32_t offset = info->outSymbol()->fragRef()->getOutputOffset(M);
    m_Addend += offset;
  }
}

bool Relocation::shouldUsePLTAddr() const {
  return m_pSymInfo->isGlobal() &&
         (m_pSymInfo->reserved() & Relocator::ReservePLT) != 0x0;
}

size_t Relocation::getOffset() const { return m_TargetAddress->offset(); }

std::string Relocation::getSymbolName(const ResolveInfo *R, bool DoDeMangle) {
  return R->getDecoratedName(DoDeMangle);
}

std::string Relocation::getSectionName(const ELFSection *S,
                                       const GeneralOptions &options) {
  if (!S)
    return "";
  if (S->isRelocationKind())
    S = S->getLink();
  return S->getDecoratedName(options);
}

bool Relocation::isMergeKind() const {
  FragmentRef *Target =
      targetFragRef() ? targetFragRef() : symInfo()->outSymbol()->fragRef();
  return Target && Target->frag() &&
         Target->frag()->getOwningSection()->isMergeKind();
}
