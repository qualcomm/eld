//===- ELFSegmentFactory.h-------------------------------------------------===//
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
#ifndef ELD_TARGET_ELFSEGMENTFACTORY_H
#define ELD_TARGET_ELFSEGMENTFACTORY_H

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/DataTypes.h"
#include <vector>

namespace eld {

class ELFSegment;
class ELFSection;

/** \class ELFSegmentFactory
 *  \brief provide the interface to create and delete an ELFSegment
 */
class ELFSegmentFactory {
public:
  typedef std::vector<ELFSegment *> Segments;
  typedef Segments::const_iterator const_iterator;
  typedef Segments::iterator iterator;
  typedef Segments::reverse_iterator riterator;

  iterator begin() { return m_Segments.begin(); }
  iterator end() { return m_Segments.end(); }

  ELFSegment *front() { return m_Segments.front(); }
  ELFSegment *back() { return m_Segments.back(); }

  void clear() { m_Segments.clear(); }

  bool empty() const { return m_Segments.empty(); }

  ELFSegment *find(uint32_t pType);

  std::vector<ELFSegment *> getSegments(uint32_t pType) const;

  ELFSegment *find(uint32_t pType, uint32_t pFlagSet, uint32_t pFlagClear);

  ELFSegment *findr(uint32_t pType, uint32_t pFlagSet, uint32_t pFlagClear);

  iterator find(uint32_t pType, const ELFSection *pSection);

  void erase(iterator pSegment);

  void push_back(ELFSegment *seg) { m_Segments.push_back(seg); }

  Segments &segments() { return m_Segments; }

  void addSegment(ELFSegment *E) { m_Segments.push_back(E); }

  const Segments &segments() const { return m_Segments; }

  void sortSegments();

  uint32_t size() const;

private:
  Segments m_Segments;
};

} // namespace eld

#endif
