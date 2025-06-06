//===- ELFFileFormat.h-----------------------------------------------------===//
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
#ifndef ELD_TARGET_ELFFILEFORMAT_H
#define ELD_TARGET_ELFFILEFORMAT_H
#include "eld/Readers/ELFSection.h"
#include "eld/Target/LDFileFormat.h"

namespace eld {

class ELFFileFormat : public LDFileFormat {
public:
  ELFFileFormat();

  void initStdSections(Module &pModule, unsigned int pBitClass);

  ELFSection *getDynamic() const { return f_pDynamic; }

  ELFSection *getDynStrTab() const { return f_pDynStrTab; }

  ELFSection *getDynSymTab() const { return f_pDynSymTab; }

  ELFSection *getShStrTab() const { return f_pShStrTab; }

  ELFSection *getStrTab() const { return f_pStrTab; }

  ELFSection *getSymTab() const { return f_pSymTab; }

  ELFSection *getSymTabShndxr() const { return f_pSymTabShndxr; }

  bool hasDynamic() const { return (f_pDynamic) && (!f_pDynamic->isIgnore()); }

  bool hasDynStrTab() const {
    return (f_pDynStrTab) && (!f_pDynStrTab->isIgnore());
  }

  bool hasDynSymTab() const {
    return (f_pDynSymTab) && (!f_pDynSymTab->isIgnore());
  }

  bool hasShStrTab() const {
    return (f_pShStrTab) && (!f_pShStrTab->isIgnore());
  }

  bool hasStrTab() const { return (f_pStrTab) && (!f_pStrTab->isIgnore()); }

  bool hasSymTab() const { return (f_pSymTab) && (!f_pSymTab->isIgnore()); }

  bool hasSymTabShndxr() const {
    return (f_pSymTabShndxr) && (!f_pSymTabShndxr->isIgnore());
  }

  std::vector<ELFSection *> &getSections() { return f_pSections; }

  std::size_t addStringToDynStrTab(const std::string &S) {
    return DynamicStringTableContents.addString(S);
  }

  std::size_t getDynStrTabSize() const {
    return DynamicStringTableContents.size();
  }

  std::optional<std::size_t> getOffsetInDynStrTab(const std::string &S) const {
    return DynamicStringTableContents.getOffset(S);
  }

  const std::string &getDynStrTabContents() const {
    return DynamicStringTableContents.Strings;
  }

private:
  struct StringTableContents {
    std::string Strings = std::string(1, '\0');
    std::unordered_map<std::string, std::size_t> StringOffsets;

    std::size_t addString(const std::string &S) {
      auto iter = StringOffsets.find(S);
      if (iter != StringOffsets.end())
        return iter->second;
      std::size_t Offset = Strings.size();
      Strings += S + std::string(1, '\0');
      return StringOffsets[S] = Offset;
    }

    std::size_t size() const { return Strings.size(); }

    std::optional<std::size_t> getOffset(const std::string &S) const {
      auto iter = StringOffsets.find(S);
      if (iter != StringOffsets.end())
        return iter->second;
      return std::nullopt;
    }
  };

private:
  ELFSection *createFileFormatSection(Module &pModule, llvm::StringRef pName,
                                      LDFileFormat::Kind pKind, uint32_t pType,
                                      uint32_t pFlag, uint32_t pAlign);
  ELFSection *f_pDynamic;      // .dynamic
  ELFSection *f_pDynStrTab;    // .dynstr
  ELFSection *f_pDynSymTab;    // .dynsym
  ELFSection *f_pShStrTab;     // .shstrtab
  ELFSection *f_pStrTab;       // .strtab
  ELFSection *f_pSymTab;       // .symtab
  ELFSection *f_pSymTabShndxr; // .symtab_shndxr

  // House all the sections in this vector for convenient iteration
  std::vector<ELFSection *> f_pSections;
  StringTableContents DynamicStringTableContents;
};

} // namespace eld

#endif
