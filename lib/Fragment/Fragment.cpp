//===- Fragment.cpp--------------------------------------------------------===//
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
#include "eld/Fragment/Fragment.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Object/SectionMap.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/raw_ostream.h"
#include <mutex>
#include <string>

using namespace eld;

//===----------------------------------------------------------------------===//
// Fragment
//===----------------------------------------------------------------------===//
Fragment::Fragment()
    : UnalignedOffset(~uint32_t(0)), m_Kind((Type)~0),
      m_pOwningSection(nullptr), m_Alignment(1) {}

Fragment::Fragment(Type pKind, ELFSection *pSection, uint32_t align)
    : UnalignedOffset(~uint32_t(0)), m_Kind(pKind), m_pOwningSection(pSection),
      m_Alignment((align == 0) ? 1 : align) {}

Fragment::~Fragment() {}

uint32_t Fragment::getOffset(DiagnosticEngine *DiagEngine) const {
  if (DiagEngine && !hasOffset()) {
    auto *I = getOwningSection()->getMatchedLinkerScriptRule();
    std::string RuleStr;
    if (I) {
      llvm::raw_string_ostream S(RuleStr);
      if ((reinterpret_cast<RuleContainer *>(I))->desc())
        (reinterpret_cast<RuleContainer *>(I))
            ->desc()
            ->dumpMap(S, false, false);
    }
    if (getOwningSection()->isDiscard()) {
      DiagEngine->raise(diag::offset_not_assigned)
          << getOwningSection()->name() << "Discarded"
          << getOwningSection()->getInputFile()->getInput()->decoratedPath();
    } else if (!I)
      DiagEngine->raise(diag::offset_not_assigned)
          << getOwningSection()->name() << getOutputELFSection()->name()
          << getOwningSection()->getInputFile()->getInput()->decoratedPath();
    else
      DiagEngine->raise(diag::offset_not_assigned_with_rule)
          << getOwningSection()->name()
          << getOwningSection()->getInputFile()->getInput()->decoratedPath()
          << RuleStr << getOutputELFSection()->name();
  }
  return UnalignedOffset + paddingSize();
}

bool Fragment::hasOffset() const { return (UnalignedOffset != ~uint32_t(0)); }

llvm::SmallVectorImpl<Fragment *>::iterator Fragment::getIterator() {
  auto &Fragments = getOwningSection()
                        ->getMatchedLinkerScriptRule()
                        ->getSection()
                        ->getFragmentList();
  auto Iter = std::find(Fragments.begin(), Fragments.end(), this);
  return Iter;
}

Fragment *Fragment::getPrevNode() {
  auto &Fragments = getOwningSection()
                        ->getMatchedLinkerScriptRule()
                        ->getSection()
                        ->getFragmentList();
  auto Begin = Fragments.begin();
  auto Iter = std::find(Begin, Fragments.end(), this);
  assert(Iter != Fragments.end());
  if (Iter == Begin)
    return nullptr;
  --Iter;
  return *Iter;
}

Fragment *Fragment::getNextNode() {
  auto &Fragments = getOwningSection()
                        ->getMatchedLinkerScriptRule()
                        ->getSection()
                        ->getFragmentList();
  auto End = Fragments.end();
  auto Iter = std::find(Fragments.begin(), End, this);
  assert(Iter != Fragments.end());
  ++Iter;
  if (Iter == End)
    return nullptr;
  return *Iter;
}

size_t Fragment::paddingSize() const {
  if (!hasOffset())
    return 0;
  return llvm::offsetToAlignment(UnalignedOffset, llvm::Align(m_Alignment));
}

void Fragment::setOffset(uint32_t pOffset) {
  if (isNull())
    return;
  UnalignedOffset = pOffset;
  if (m_pOwningSection) {
    m_pOwningSection->setOffsetAndAddr(UnalignedOffset + paddingSize());
    // Dont count inputs that dont have allocatable sections
    if (m_pOwningSection->isAlloc() && m_pOwningSection->getInputFile() &&
        !m_pOwningSection->getInputFile()->isUsed())
      m_pOwningSection->getInputFile()->setUsed(true);
  }
}

uint32_t Fragment::getNewOffset(uint32_t pOffset) const {
  return (pOffset + llvm::offsetToAlignment(pOffset, llvm::Align(m_Alignment)));
}

ELFSection *Fragment::getOutputELFSection() const {
  return m_pOwningSection->getOutputELFSection();
}

// Pass DiagEngine here.
uint64_t Fragment::getAddr(DiagnosticEngine *DiagEngine) const {
  return getOutputELFSection()->addr() + getOffset(DiagEngine);
}

bool Fragment::isMergeStr() const { return getOwningSection()->isMergeKind(); }

bool Fragment::originatesFromPlugin(const Module &module) const {
  return getOwningSection()->getInputFile() ==
         module.getInternalInput(Module::InternalInputType::Plugin);
}
