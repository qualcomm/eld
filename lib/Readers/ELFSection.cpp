//===- ELFSection.cpp------------------------------------------------------===//
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

#include "eld/Readers/ELFSection.h"
#include "llvm/ADT/StringRef.h"

using namespace eld;

bool ELFSection::hasOffset() const { return (Offset != ~uint64_t(0)); }

bool ELFSection::hasSectionData() const { return (Fragments.size() > 0); }

void ELFSection::addFragment(Fragment *F) {
  // Update Owning section.
  if (!F->getOwningSection())
    F->setOwningSection(this);
  // Update Alignment if necessary.
  if (AddrAlign < F->alignment())
    AddrAlign = F->alignment();
  /// LDFileFormat::MergeStr sections must consist of a single Fragment
  if (isMergeKind())
    assert(Fragments.empty());
  Fragments.push_back(F);
}

void ELFSection::addFragmentAndUpdateSize(Fragment *F) {
  addFragment(F);
  m_Size += F->size();
}

#define INTTOELFTSTR(ns, enum)                                                 \
  case ns::enum:                                                               \
    return #enum;

llvm::StringRef ELFSection::getELFTypeStr(llvm::StringRef Name, uint32_t Type) {
  switch (Type) {
    INTTOELFTSTR(llvm::ELF, SHT_NULL);
    INTTOELFTSTR(llvm::ELF, SHT_PROGBITS);
    INTTOELFTSTR(llvm::ELF, SHT_SYMTAB);
    INTTOELFTSTR(llvm::ELF, SHT_STRTAB);
    INTTOELFTSTR(llvm::ELF, SHT_RELA);
    INTTOELFTSTR(llvm::ELF, SHT_HASH);
    INTTOELFTSTR(llvm::ELF, SHT_DYNAMIC);
    INTTOELFTSTR(llvm::ELF, SHT_NOTE);
    INTTOELFTSTR(llvm::ELF, SHT_NOBITS);
    INTTOELFTSTR(llvm::ELF, SHT_REL);
    INTTOELFTSTR(llvm::ELF, SHT_SHLIB);
    INTTOELFTSTR(llvm::ELF, SHT_DYNSYM);
    INTTOELFTSTR(llvm::ELF, SHT_INIT_ARRAY);
    INTTOELFTSTR(llvm::ELF, SHT_FINI_ARRAY);
    INTTOELFTSTR(llvm::ELF, SHT_PREINIT_ARRAY);
    INTTOELFTSTR(llvm::ELF, SHT_GROUP);
    INTTOELFTSTR(llvm::ELF, SHT_SYMTAB_SHNDX);
    INTTOELFTSTR(llvm::ELF, SHT_GNU_ATTRIBUTES);
    INTTOELFTSTR(llvm::ELF, SHT_GNU_HASH);
    INTTOELFTSTR(llvm::ELF, SHT_GNU_verdef);
    INTTOELFTSTR(llvm::ELF, SHT_GNU_verneed);
    INTTOELFTSTR(llvm::ELF, SHT_GNU_versym);
    INTTOELFTSTR(llvm::ELF, SHT_ARM_EXIDX);
    INTTOELFTSTR(llvm::ELF, SHT_ARM_PREEMPTMAP);
    INTTOELFTSTR(llvm::ELF, SHT_ARM_DEBUGOVERLAY);
    INTTOELFTSTR(llvm::ELF, SHT_ARM_OVERLAYSECTION);
  default:
    break;
  }
  // The attribute type is overloaded.
  if (Type == llvm::ELF::SHT_RISCV_ATTRIBUTES && Name.starts_with(".riscv"))
    return "SHT_RISCV_ATTRIBUTES";
  if (Type == llvm::ELF::SHT_ARM_ATTRIBUTES && Name.starts_with(".ARM"))
    return "SHT_ARM_ATTRIBUTES";
  if (Type == llvm::ELF::SHT_HEXAGON_ATTRIBUTES && Name.starts_with(".hexagon"))
    return "SHT_HEXAGON_ATTRIBUTES";
  return "Not Defined";
}

std::string ELFSection::getELFPermissionsStr(uint32_t permissions) {
#define INTOPERMISSIONSSTR(ns, enum)                                           \
  if (permissions & ns::enum) {                                                \
    if (!elfPermStr.empty())                                                   \
      elfPermStr += "|";                                                       \
    elfPermStr += #enum;                                                       \
  }

  std::string elfPermStr;
  INTOPERMISSIONSSTR(llvm::ELF, SHF_ALLOC);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_WRITE);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_LINK_ORDER);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_EXECINSTR);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_MERGE);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_STRINGS);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_GROUP);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_HEX_GPREL);
  INTOPERMISSIONSSTR(llvm::ELF, SHF_TLS);

  if (elfPermStr.empty())
    elfPermStr = "NONE";
  return elfPermStr;
}

// If an input section is in the form of "foo.N" where N is a number,
// return N. Otherwise, returns 65536, which is one greater than the
// lowest priority.
int ELFSection::getPriority() const {
  llvm::StringRef S = name();
  size_t Pos = S.rfind('.');
  if (Pos == llvm::StringRef::npos)
    return 65536;
  int V;
  if (S.substr(Pos + 1).getAsInteger(10, V))
    return 65536;
  return V;
}

Relocation *ELFSection::createOneReloc() {
  Relocation *R = Relocation::Create();
  addRelocation(R);
  return R;
}

Relocation *ELFSection::findRelocation(uint64_t Offset, Relocation::Type Type,
                                       bool Reverse) const {
  if (Reverse) {
    auto Reloc = std::find_if(
        Relocations.rbegin(), Relocations.rend(), [&](Relocation *R) -> bool {
          return R->type() == Type && !R->targetRef()->isNull() &&
                 R->targetRef()->offset() == Offset;
        });
    if (Reloc != Relocations.rend())
      return *Reloc;
  } else {
    const auto *Reloc = std::find_if(
        Relocations.begin(), Relocations.end(), [&](Relocation *R) -> bool {
          return R->type() == Type && !R->targetRef()->isNull() &&
                 R->targetRef()->offset() == Offset;
        });
    if (Reloc != Relocations.end())
      return *Reloc;
  }
  return nullptr;
}

Fragment *ELFSection::getFirstFragmentInRule() const {
  OutputSectionEntry *OE = getOutputSection();
  if (!OE || !OE->size())
    return nullptr;
  for (auto &In : *OE) {
    if (In->getSection()->size())
      return In->getSection()->getFragmentList().front();
  }
  return nullptr;
}

void ELFSection::setOffsetAndAddr(uint64_t Off) {
  Offset = Off;
  OutputSectionEntry *OE = getOutputSection();
  if (!OE)
    return;
  Addr = Offset + OE->getSection()->addr();
  PAddr = Offset + OE->getSection()->pAddr();
}

std::string ELFSection::getDecoratedName(const GeneralOptions &options) const {
  std::string decoratedName = m_Name;
  if (!options.shouldShowRMSectNameInDiag())
    return decoratedName;
  std::optional<std::string> RMSectName;
  RMSectName = getRMSectName();
  if (RMSectName)
    decoratedName += "(" + RMSectName.value() + ")";
  return decoratedName;
}

std::optional<std::string> ELFSection::getRMSectName() const {
  std::optional<std::string> RMSectName;
  if (const ObjectFile *objFile =
          llvm::dyn_cast_or_null<const ObjectFile>(m_InputFile)) {
    RMSectName = objFile->getRuleMatchingSectName(getIndex());
  }
  return RMSectName;
}

std::string ELFSection::getLocation(uint64_t Offset,
                                    const GeneralOptions &Options) const {
  std::string SectionName = getDecoratedName(Options);
  std::string SectionAndOffset = SectionName;
  if (Offset) {
    SectionAndOffset += "+0x";
    SectionAndOffset += llvm::Twine::utohexstr(Offset).str();
  }
  SectionAndOffset.push_back(')');

  if (InputFile *F = originalInput()) {
    if (auto *I = F->getInput())
      return I->decoratedPath() + ":(" + SectionAndOffset;
  }
  return std::string(name()) + ":(" + SectionAndOffset;
}