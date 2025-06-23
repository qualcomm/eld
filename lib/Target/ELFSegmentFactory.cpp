//===- ELFSegmentFactory.cpp-----------------------------------------------===//
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
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Object/SectionMap.h"
#include "eld/Target/ELFSegment.h"
#include <algorithm>

using namespace eld;

//===----------------------------------------------------------------------===//
// ELFSegmentFactory
//===----------------------------------------------------------------------===//

ELFSegment *ELFSegmentFactory::find(uint32_t pType) {
  for (auto &segment : m_Segments) {
    if (segment->type() == pType)
      return segment;
  }
  return nullptr;
}

ELFSegment *ELFSegmentFactory::find(uint32_t pType, uint32_t pFlagSet,
                                    uint32_t pFlagClear) {
  for (auto &segment : m_Segments) {
    if (segment->type() == pType && (segment->flag() & pFlagSet) == pFlagSet &&
        (segment->flag() & pFlagClear) == 0x0) {
      return segment;
    }
  }
  return nullptr;
}

std::vector<ELFSegment *> ELFSegmentFactory::getSegments(uint32_t pType) const {
  std::vector<ELFSegment *> ELFSegments;
  for (auto &segment : m_Segments) {
    if (segment->type() == pType)
      ELFSegments.push_back(segment);
  }
  return ELFSegments;
}

ELFSegment *ELFSegmentFactory::findr(uint32_t pType, uint32_t pFlagSet,
                                     uint32_t pFlagClear) {
  riterator segment, segEnd = m_Segments.rend();
  for (segment = m_Segments.rbegin(); segment != segEnd; ++segment) {
    if ((*segment)->type() == pType &&
        ((*segment)->flag() & pFlagSet) == pFlagSet &&
        ((*segment)->flag() & pFlagClear) == 0x0) {
      return *segment;
    }
  }
  return nullptr;
}

ELFSegmentFactory::iterator
ELFSegmentFactory::find(uint32_t pType, const ELFSection *pSection) {
  iterator segment, segEnd = end();
  for (segment = begin(); segment != segEnd; ++segment) {
    if ((*segment)->type() == pType) {
      ELFSegment::iterator sect, sectEnd = (*segment)->end();
      for (sect = (*segment)->begin(); sect != sectEnd; ++sect) {
        if ((*sect)->getSection() == pSection)
          return segment;
      } // for each section
    }
  } // for each segment
  return segEnd;
}

void ELFSegmentFactory::erase(iterator pSegment) { m_Segments.erase(pSegment); }

uint32_t ELFSegmentFactory::size() const { return m_Segments.size(); }

void ELFSegmentFactory::sortSegments() {
  std::stable_sort(m_Segments.begin(), m_Segments.end(),
                   [](ELFSegment *s1, ELFSegment *s2) {
                     uint64_t type1 = s1->type();
                     uint64_t type2 = s2->type();

                     if (type1 == type2)
                       return false;

                     // The single PT_PHDR segment is required to precede any
                     // loadable
                     // segment.  We simply make it always first.
                     if (type1 == llvm::ELF::PT_PHDR)
                       return true;
                     if (type2 == llvm::ELF::PT_PHDR)
                       return false;

                     // The single PT_INTERP segment is required to precede any
                     // loadable
                     // segment.  We simply make it always second.
                     if (type1 == llvm::ELF::PT_INTERP)
                       return true;
                     if (type2 == llvm::ELF::PT_INTERP)
                       return false;

                     // We then put PT_LOAD segments before any other segments.
                     if (type1 == llvm::ELF::PT_LOAD)
                       return true;
                     if (type2 == llvm::ELF::PT_LOAD)
                       return false;

                     // We put the PT_GNU_RELRO segment last, because that is
                     // where the
                     // dynamic linker expects to find it
                     if (type1 == llvm::ELF::PT_GNU_RELRO)
                       return false;
                     if (type2 == llvm::ELF::PT_GNU_RELRO)
                       return true;

                     // We put the PT_TLS segment last except for the
                     // PT_GNU_RELRO
                     // segment, because that is where the dynamic linker
                     // expects to
                     // find
                     if (type1 == llvm::ELF::PT_TLS)
                       return false;
                     if (type2 == llvm::ELF::PT_TLS)
                       return true;

                     // Otherwise compare the types to establish an arbitrary
                     // ordering.
                     // FIXME: Should figure out if we should just make all
                     // other types
                     // compare
                     // equal, but if so, we should probably do the same for
                     // atom flags
                     // and
                     // change
                     // users of this to use stable_sort.
                     return type1 < type2;
                   });
}
