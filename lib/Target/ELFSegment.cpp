//===- ELFSegment.cpp------------------------------------------------------===//
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
#include "eld/Target/ELFSegment.h"
#include "eld/Config/Config.h"
#include "eld/Readers/ELFSection.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ELFSegment
//===----------------------------------------------------------------------===//
ELFSegment::ELFSegment(uint32_t pType, uint32_t pFlag, Expression *E,
                       const PhdrSpec *Spec)
    : m_Type(pType), m_Flag(pFlag), m_Offset(0x0), m_Vaddr(0x0), m_Paddr(0x0),
      m_Filesz(0x0), m_Memsz(0x0), m_Align(0x0), m_MaxSectionAlign(0x0),
      m_Ordinal(0x0), m_AtAddress(0), m_Spec(Spec) {
  if (E)
    setFixedLMA(E);
}

bool ELFSegment::isLoadSegment() const { return type() == llvm::ELF::PT_LOAD; }

ELFSegment::iterator ELFSegment::insert(ELFSegment::iterator pPos,
                                        OutputSectionEntry *oSection) {
  return m_SectionList.insert(pPos, oSection);
}

void ELFSegment::append(OutputSectionEntry *oSection) {
  ELFSection *pSection = oSection->getSection();
  assert(nullptr != pSection);
  if (pSection->getAddrAlign() > m_MaxSectionAlign)
    m_MaxSectionAlign = pSection->getAddrAlign();
  m_SectionList.push_back(oSection);
  if (isLoadSegment())
    oSection->setLoadSegment(this);
}

void ELFSegment::sortSections() {
  std::stable_sort(m_SectionList.begin(), m_SectionList.end(),
                   [](OutputSectionEntry *a, OutputSectionEntry *b) {
                     return a->getSection()->addr() < b->getSection()->addr();
                   });
}

llvm::StringRef ELFSegment::TypeToELFTypeStr(uint32_t type) {
#define INTOELFTSTR(ns, enum)                                                  \
  case ns::enum:                                                               \
    return #enum;

  switch (type) {
    INTOELFTSTR(llvm::ELF, PT_NULL);
    INTOELFTSTR(llvm::ELF, PT_LOAD);
    INTOELFTSTR(llvm::ELF, PT_DYNAMIC);
    INTOELFTSTR(llvm::ELF, PT_INTERP);
    INTOELFTSTR(llvm::ELF, PT_NOTE);
    INTOELFTSTR(llvm::ELF, PT_SHLIB);
    INTOELFTSTR(llvm::ELF, PT_PHDR);
    INTOELFTSTR(llvm::ELF, PT_ARM_EXIDX);
    INTOELFTSTR(llvm::ELF, PT_GNU_EH_FRAME);
    INTOELFTSTR(llvm::ELF, PT_TLS);
    INTOELFTSTR(llvm::ELF, PT_GNU_STACK);
  default:
    break;
  }
  return "Not Defined";
}

std::string ELFSegment::PermissionToELFPermissionsStr(uint32_t permissions) {
#define INTOPERMISSIONSSTR(ns, enum)                                           \
  if (permissions & ns::enum) {                                                \
    if (!elfPermStr.empty())                                                   \
      elfPermStr += "|";                                                       \
    elfPermStr += #enum;                                                       \
  }
  std::string elfPermStr;
  INTOPERMISSIONSSTR(llvm::ELF, PF_R);
  INTOPERMISSIONSSTR(llvm::ELF, PF_W);
  INTOPERMISSIONSSTR(llvm::ELF, PF_X);
  INTOPERMISSIONSSTR(llvm::ELF, PF_MASKOS);
  INTOPERMISSIONSSTR(llvm::ELF, PF_MASKPROC);
  return elfPermStr;
}
