//===- FragmentRef.cpp-----------------------------------------------------===//
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
#include "eld/Fragment/FragmentRef.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/GOT.h"
#include "eld/Fragment/OutputSectDataFragment.h"
#include "eld/Fragment/PLT.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/RegionFragmentEx.h"
#include "eld/Fragment/StringFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/EhFrameSection.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"

using namespace eld;

FragmentRef FragmentRef::g_NullFragmentRef;
FragmentRef FragmentRef::g_DiscardFragmentRef;

//===----------------------------------------------------------------------===//
// FragmentRef
//===----------------------------------------------------------------------===//
FragmentRef::FragmentRef() : m_pFragment(nullptr), m_Offset(0) {}

FragmentRef::FragmentRef(Fragment &pFrag, FragmentRef::Offset pOffset)
    : m_pFragment(&pFrag), m_Offset(pOffset) {}

FragmentRef *FragmentRef::Null() { return &g_NullFragmentRef; }

FragmentRef *FragmentRef::Discard() { return &g_DiscardFragmentRef; }

void FragmentRef::memcpy(void *pDest, size_t pNBytes, Offset pOffset) const {
  // check if the offset is still in a legal range.
  if (nullptr == m_pFragment)
    return;
  unsigned int total_offset = m_Offset + pOffset;
  switch (m_pFragment->getKind()) {
  /// FIXME: there is lots of unecessary code duplication here
  case Fragment::MergeString: {
    auto *Strings = static_cast<MergeStringFragment *>(m_pFragment);
    unsigned int total_length = Strings->size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;
    Strings->copyData(pDest, pNBytes, total_offset);
    return;
  }
  case Fragment::Region: {
    RegionFragment *region_frag = static_cast<RegionFragment *>(m_pFragment);
    unsigned int total_length = region_frag->getRegion().size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;

    region_frag->copyData(pDest, pNBytes, total_offset);
    return;
  }
  case Fragment::RegionFragmentEx: {
    RegionFragmentEx *region_frag =
        static_cast<RegionFragmentEx *>(m_pFragment);
    unsigned int total_length = region_frag->getRegion().size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;

    region_frag->copyData(pDest, pNBytes, total_offset);
    return;
  }
  case Fragment::String: {
    StringFragment *string_frag = static_cast<StringFragment *>(m_pFragment);
    unsigned int total_length = string_frag->getString().size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;

    std::memcpy(pDest, string_frag->getString().c_str() + total_offset,
                pNBytes);
    return;
  }
  case Fragment::Stub: {
    Stub *stub_frag = static_cast<Stub *>(m_pFragment);
    unsigned int total_length = stub_frag->size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;
    std::memcpy(pDest, stub_frag->getContent() + total_offset, pNBytes);
    return;
  }
  case Fragment::Plt: {
    PLT *P = static_cast<PLT *>(m_pFragment);
    unsigned int total_length = P->size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;
    std::memcpy(pDest, P->getContent().data() + total_offset, pNBytes);
    return;
  }
  case Fragment::Got: {
    GOT *G = static_cast<GOT *>(m_pFragment);
    unsigned int total_length = G->size();
    if (total_length < (total_offset + pNBytes))
      pNBytes = total_length - total_offset;
    std::memcpy(pDest, G->getContent().data() + total_offset, pNBytes);
    return;
  }
  case Fragment::OutputSectDataFragType:
    ASSERT(false, "OutputSectDataFragment cannot be copied!");
  case Fragment::Fillment:
  default:
    return;
  }
}

FragmentRef::Offset FragmentRef::getOutputOffset(Module &M) const {
  if (m_pFragment->getOwningSection()->getKind() == LDFileFormat::EhFrame) {
    // Find the proper piece
    EhFrameSection *S =
        llvm::dyn_cast<eld::EhFrameSection>(m_pFragment->getOwningSection());
    std::vector<EhFramePiece> &Pieces = S->getPieces();
    size_t I = 0;
    while (I != Pieces.size() &&
           Pieces[I].getOffset() + Pieces[I].getSize() <= m_Offset)
      ++I;
    if (I == Pieces.size())
      return m_Offset + Pieces.back().getOutputOffset() -
             Pieces.back().getOffset();

    if (!Pieces[I].hasOutputOffset())
      return -1;
    return m_Offset + Pieces[I].getOutputOffset() - Pieces[I].getOffset();
  }
  /// Correct the output offset for merged strings
  if (m_pFragment->isMergeStr()) {
    auto *MSF = llvm::cast<MergeStringFragment>(m_pFragment);
    OutputSectionEntry *O = getOutputSection();
    MergeableString *S = MSF->findString(m_Offset);
    assert(S);
    /// The target string could be a suffix
    uint32_t OffsetInString = m_Offset - S->InputOffset;
    bool GlobalMerge = M.getConfig().options().shouldGlobalStringMerge();
    if (const MergeableString *Merged = (!S->isAlloc() && GlobalMerge)
                                            ? M.getMergedNonAllocString(S)
                                            : O->getMergedString(S)) {
      assert(S->Exclude);
      assert(!Merged->Exclude);
      assert(Merged->hasOutputOffset());
      return Merged->OutputOffset + OffsetInString;
    }
    assert(!S->Exclude);
    assert(S->hasOutputOffset());
    return S->OutputOffset + OffsetInString;
  }
  Offset result = 0;
  if (nullptr != m_pFragment)
    result = m_pFragment->getOffset(M.getConfig().getDiagEngine());
  return (result + m_Offset);
}

ELFSection *FragmentRef::getOutputELFSection() const {
  if (isNull() || isDiscard())
    return nullptr;
  return frag()->getOutputELFSection();
}

OutputSectionEntry *FragmentRef::getOutputSection() const {
  ELFSection *ES = getOutputELFSection();
  if (!ES)
    return nullptr;
  return ES->getOutputSection();
}
