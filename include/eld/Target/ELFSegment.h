//===- ELFSegment.h--------------------------------------------------------===//
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
#ifndef ELD_TARGET_ELFSEGMENT_H
#define ELD_TARGET_ELFSEGMENT_H
#include "eld/Config/Config.h"
#include "eld/Object/OutputSectionEntry.h"
#include "eld/Object/RuleContainer.h"
#include "eld/Object/SectionMap.h"
#include "eld/Script/Expression.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/DataTypes.h"
#include <string>
#include <vector>

namespace eld {

class ELFSection;
struct PhdrSpec;

/** \class ELFSegment
 *  \brief decribe the program header for ELF executable or shared object
 */
class ELFSegment {
public:
  typedef std::vector<OutputSectionEntry *> SectionList;
  typedef SectionList::iterator iterator;
  typedef SectionList::const_iterator const_iterator;
  typedef SectionList::reverse_iterator reverse_iterator;
  typedef SectionList::const_reverse_iterator const_reverse_iterator;

  explicit ELFSegment(uint32_t pType, uint32_t pFlag = llvm::ELF::PF_R,
                      Expression *F = nullptr, const PhdrSpec *Spec = nullptr);

public:
  ~ELFSegment() {}

  ///  -----  iterators  -----  ///
  iterator begin() { return m_SectionList.begin(); }
  const_iterator begin() const { return m_SectionList.begin(); }
  iterator end() { return m_SectionList.end(); }
  const_iterator end() const { return m_SectionList.end(); }

  reverse_iterator rbegin() { return m_SectionList.rbegin(); }
  const_reverse_iterator rbegin() const { return m_SectionList.rbegin(); }
  reverse_iterator rend() { return m_SectionList.rend(); }
  const_reverse_iterator rend() const { return m_SectionList.rend(); }

  ELFSection *front() { return m_SectionList.front()->getSection(); }
  const ELFSection *front() const {
    return m_SectionList.front()->getSection();
  }
  ELFSection *back() { return m_SectionList.back()->getSection(); }
  const ELFSection *back() const { return m_SectionList.back()->getSection(); }

  ///  -----  observers  -----  ///
  uint32_t type() const { return m_Type; }
  uint64_t offset() const { return m_Offset; }
  uint64_t vaddr() const { return m_Vaddr; }
  uint64_t paddr() const { return m_Paddr; }
  uint64_t filesz() const { return m_Filesz; }
  uint64_t memsz() const { return m_Memsz; }
  uint32_t flag() const { return m_Flag; }
  uint64_t align() const { return m_Align; }

  // If the linker script requires this segment to have a fixed LMA address,
  // this function returns true.
  bool hasFixedLMA() const { return (m_AtAddress != nullptr); }

  // Return the expression of the fixed segment address specified.
  Expression *fixedLMA() const { return m_AtAddress; }

  // Set the fixed LMA to the expression specified by the user.
  void setFixedLMA(Expression *E) { m_AtAddress = E; }

  size_t size() const { return m_SectionList.size(); }
  bool empty() const { return m_SectionList.empty(); }
  void clear() { m_SectionList.clear(); }

  bool isLoadSegment() const;
  uint64_t segAlign() const { return m_Align; }

  ///  -----  modifiers  -----  ///
  void setOffset(uint64_t pOffset) { m_Offset = pOffset; }

  void setVaddr(uint64_t pVaddr) { m_Vaddr = pVaddr; }

  void setPaddr(uint64_t pPaddr) { m_Paddr = pPaddr; }

  void setFilesz(uint64_t pFilesz) { m_Filesz = pFilesz; }

  void setMemsz(uint64_t pMemsz) { m_Memsz = pMemsz; }

  void setFlag(uint32_t pFlag) { m_Flag = pFlag; }

  void setName(std::string name) { m_Name = name; }

  std::string name() const { return m_Name; }

  void updateFlag(uint32_t pFlag) {
    // PT_TLS segment should be PF_R
    if (llvm::ELF::PT_TLS != m_Type)
      m_Flag |= pFlag;
  }

  void updateFlagPhdr(uint32_t pFlag) { m_Flag |= pFlag; }

  SectionList &sections() { return m_SectionList; }

  const SectionList &sections() const { return m_SectionList; }

  void setAlign(uint64_t pAlign) { m_Align = pAlign; }

  void setOrdinal(uint16_t ordinal) { m_Ordinal = ordinal; }

  uint16_t getOrdinal() { return m_Ordinal; }

  uint64_t getMaxSectionAlign() const { return m_MaxSectionAlign; }

  iterator insert(iterator pPos, OutputSectionEntry *pSection);

  void append(OutputSectionEntry *pSection);

  void sortSections();

  const PhdrSpec *getSpec() const { return m_Spec; }

  // -----------------------Segment Helper functions ----------------------
  static llvm::StringRef TypeToELFTypeStr(uint32_t);

  static std::string PermissionToELFPermissionsStr(uint32_t);

  // ---------------------NONE Segment support ----------------------------
  bool isNoneSegment() const {
    return llvm::StringRef(m_Name).lower() == "none";
  }

private:
  std::string m_Name;         // Name of the segment
  uint32_t m_Type;            // Type of segment
  uint32_t m_Flag;            // Segment flags
  uint64_t m_Offset;          // File offset where segment is located, in bytes
  uint64_t m_Vaddr;           // Virtual address of the segment
  uint64_t m_Paddr;           // Physical address of the segment (OS-specific)
  uint64_t m_Filesz;          // # of bytes in file image of segment (may be 0)
  uint64_t m_Memsz;           // # of bytes in mem image of segment (may be 0)
  uint64_t m_Align;           // alignment constraint
  uint64_t m_MaxSectionAlign; // max alignment of the sections in this segment
  uint16_t m_Ordinal;
  Expression *m_AtAddress;
  const PhdrSpec *m_Spec;
  SectionList m_SectionList;
};

} // namespace eld

#endif
