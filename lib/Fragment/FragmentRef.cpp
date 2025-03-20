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

FragmentRef FragmentRef::GNullFragmentRef;
FragmentRef FragmentRef::GDiscardFragmentRef;

//===----------------------------------------------------------------------===//
// FragmentRef
//===----------------------------------------------------------------------===//
FragmentRef::FragmentRef() : ThisFragment(nullptr), ThisOffset(0) {}

FragmentRef::FragmentRef(Fragment &PFrag, FragmentRef::Offset POffset)
    : ThisFragment(&PFrag), ThisOffset(POffset) {}

FragmentRef *FragmentRef::null() { return &GNullFragmentRef; }

FragmentRef *FragmentRef::discard() { return &GDiscardFragmentRef; }

void FragmentRef::memcpy(void *PDest, size_t PNBytes, Offset POffset) const {
  // check if the offset is still in a legal range.
  if (nullptr == ThisFragment)
    return;
  unsigned int TotalOffset = ThisOffset + POffset;
  switch (ThisFragment->getKind()) {
  /// FIXME: there is lots of unecessary code duplication here
  case Fragment::MergeString: {
    auto *Strings = static_cast<MergeStringFragment *>(ThisFragment);
    unsigned int TotalLength = Strings->size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;
    Strings->copyData(PDest, PNBytes, TotalOffset);
    return;
  }
  case Fragment::Region: {
    RegionFragment *RegionFrag = static_cast<RegionFragment *>(ThisFragment);
    unsigned int TotalLength = RegionFrag->getRegion().size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;

    RegionFrag->copyData(PDest, PNBytes, TotalOffset);
    return;
  }
  case Fragment::RegionFragmentEx: {
    RegionFragmentEx *RegionFrag =
        static_cast<RegionFragmentEx *>(ThisFragment);
    unsigned int TotalLength = RegionFrag->getRegion().size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;

    RegionFrag->copyData(PDest, PNBytes, TotalOffset);
    return;
  }
  case Fragment::String: {
    StringFragment *StringFrag = static_cast<StringFragment *>(ThisFragment);
    unsigned int TotalLength = StringFrag->getString().size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;

    std::memcpy(PDest, StringFrag->getString().c_str() + TotalOffset, PNBytes);
    return;
  }
  case Fragment::Stub: {
    Stub *StubFrag = static_cast<Stub *>(ThisFragment);
    unsigned int TotalLength = StubFrag->size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;
    std::memcpy(PDest, StubFrag->getContent() + TotalOffset, PNBytes);
    return;
  }
  case Fragment::Plt: {
    PLT *P = static_cast<PLT *>(ThisFragment);
    unsigned int TotalLength = P->size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;
    std::memcpy(PDest, P->getContent().data() + TotalOffset, PNBytes);
    return;
  }
  case Fragment::Got: {
    GOT *G = static_cast<GOT *>(ThisFragment);
    unsigned int TotalLength = G->size();
    if (TotalLength < (TotalOffset + PNBytes))
      PNBytes = TotalLength - TotalOffset;
    std::memcpy(PDest, G->getContent().data() + TotalOffset, PNBytes);
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
  if (ThisFragment->getOwningSection()->getKind() == LDFileFormat::EhFrame) {
    // Find the proper piece
    EhFrameSection *S =
        llvm::dyn_cast<eld::EhFrameSection>(ThisFragment->getOwningSection());
    std::vector<EhFramePiece> &Pieces = S->getPieces();
    size_t I = 0;
    while (I != Pieces.size() &&
           Pieces[I].getOffset() + Pieces[I].getSize() <= ThisOffset)
      ++I;
    if (I == Pieces.size())
      return ThisOffset + Pieces.back().getOutputOffset() -
             Pieces.back().getOffset();

    if (!Pieces[I].hasOutputOffset())
      return -1;
    return ThisOffset + Pieces[I].getOutputOffset() - Pieces[I].getOffset();
  }
  /// Correct the output offset for merged strings
  if (ThisFragment->isMergeStr()) {
    auto *MSF = llvm::cast<MergeStringFragment>(ThisFragment);
    OutputSectionEntry *O = getOutputSection();
    MergeableString *S = MSF->findString(ThisOffset);
    assert(S);
    /// The target string could be a suffix
    uint32_t OffsetInString = ThisOffset - S->InputOffset;
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
  Offset Result = 0;
  if (nullptr != ThisFragment)
    Result = ThisFragment->getOffset(M.getConfig().getDiagEngine());
  return (Result + ThisOffset);
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
