//===- ELFDynamic.h--------------------------------------------------------===//
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
#ifndef ELD_TARGET_ELFDYNAMIC_H
#define ELD_TARGET_ELFDYNAMIC_H

#include "eld/Support/MemoryRegion.h"
#include "llvm/BinaryFormat/ELF.h"
#include <string>
#include <vector>

namespace eld {

class DynStrFragment;
class ELFSection;
class GNULDBackend;
class LinkerConfig;
class Module;

/** \class ELFDynamic
 *  \brief ELFDynamic is the .dynamic section in ELF shared and executable
 *  files.
 */
class ELFDynamic {
public:
  struct DynEntry {
    uint64_t tag = 0;
    uint64_t value = 0;
  };

  typedef std::vector<DynEntry> EntryListType;
  typedef EntryListType::iterator iterator;
  typedef EntryListType::const_iterator const_iterator;

public:
  ELFDynamic(LinkerConfig &pConfig, ELFSection &pDynSection);

  virtual ~ELFDynamic() = default;

  size_t size() const;

  size_t entrySize() const;

  size_t numOfBytes() const;

  /// reserveEntries - reserve entries
  void reserveEntries(GNULDBackend &pBackend, DynStrFragment *DynStr,
                      Module &pModule);

  /// reserveNeedEntry - reserve one DT_NEEDED/DT_RUNPATH entry.
  DynEntry *reserveNeedEntry();

  /// applyEntries - apply entries
  void applyEntries(GNULDBackend &pBackend, const ELFSection *DynStrSect,
                    const Module &pModule);

  void applySoname(uint64_t pStrTabIdx);

  const_iterator needBegin() const { return m_NeedList.begin(); }
  iterator needBegin() { return m_NeedList.begin(); }

  const_iterator needEnd() const { return m_NeedList.end(); }
  iterator needEnd() { return m_NeedList.end(); }

  /// emit
  void emit(const ELFSection &pSection, MemoryRegion &pRegion) const;

  static std::string TagToString(uint64_t Tag);

  void reserveOne(uint64_t pTag);

  void applyOne(uint64_t pTag, uint64_t pValue);

  size_t symbolSize() const;
  size_t relSize() const;
  size_t relaSize() const;

  LinkerConfig &config() const { return m_Config; }

private:
  bool is32Bits() const;

  EntryListType m_EntryList;
  EntryListType m_NeedList;
  // Counter for applyOne: entries are applied in reservation order, one-by-one.
  size_t m_Idx = 0;

  LinkerConfig &m_Config;
  ELFSection &m_DynamicSection;
};

} // namespace eld

#endif
