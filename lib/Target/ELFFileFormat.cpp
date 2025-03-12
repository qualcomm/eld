//===- ELFFileFormat.cpp---------------------------------------------------===//
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
#include "eld/Target/ELFFileFormat.h"
#include "eld/Core/Module.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

ELFFileFormat::ELFFileFormat()
    : f_pDynamic(nullptr), f_pDynStrTab(nullptr), f_pDynSymTab(nullptr),
      f_pShStrTab(nullptr), f_pStrTab(nullptr), f_pSymTab(nullptr) {}

void ELFFileFormat::initStdSections(Module &pModule, unsigned int pBitClass) {
  ELFSection *NullSection = createFileFormatSection(
      pModule, "", LDFileFormat::Null, llvm::ELF::SHT_NULL, 0x0, 0x0);
  NullSection->setOffset(0);

  if (!pModule.getConfig().isCodeStatic() ||
      pModule.getConfig().options().isPIE() ||
      pModule.getConfig().options().forceDynamic()) {
    f_pDynSymTab = createFileFormatSection(
        pModule, ".dynsym", LDFileFormat::NamePool, llvm::ELF::SHT_DYNSYM,
        llvm::ELF::SHF_ALLOC, pBitClass / 8);

    f_pDynStrTab = createFileFormatSection(
        pModule, ".dynstr", LDFileFormat::NamePool, llvm::ELF::SHT_STRTAB,
        llvm::ELF::SHF_ALLOC, 0x1);

    f_pDynamic = createFileFormatSection(
        pModule, ".dynamic", LDFileFormat::NamePool, llvm::ELF::SHT_DYNAMIC,
        llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, pBitClass / 8);
  }
  f_pShStrTab =
      createFileFormatSection(pModule, ".shstrtab", LDFileFormat::NamePool,
                              llvm::ELF::SHT_STRTAB, 0x0, 0x1);
  f_pSymTab =
      createFileFormatSection(pModule, ".symtab", LDFileFormat::NamePool,
                              llvm::ELF::SHT_SYMTAB, 0x0, pBitClass / 8);
  f_pSymTabShndxr =
      createFileFormatSection(pModule, ".symtab_shndxr", LDFileFormat::NamePool,
                              llvm::ELF::SHT_SYMTAB_SHNDX, 0x0, 4);
  f_pStrTab =
      createFileFormatSection(pModule, ".strtab", LDFileFormat::NamePool,
                              llvm::ELF::SHT_STRTAB, 0x0, 0x1);
}

ELFSection *
ELFFileFormat::createFileFormatSection(Module &pModule, llvm::StringRef pName,
                                       LDFileFormat::Kind pKind, uint32_t pType,
                                       uint32_t pFlag, uint32_t pAlign) {
  f_pSections.emplace_back(
      pModule.createOutputSection(pName.str(), pKind, pType, pFlag, pAlign));
  f_pSections.back()->setHasNoFragments();
  return f_pSections.back();
}
