//===- BinaryFileParser.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/BinaryFileParser.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Object/SectionMap.h"
#include "eld/Readers/ELFSection.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace eld;

eld::Expected<void> BinaryFileParser::parseFile(InputFile &inputFile) {
  LayoutPrinter *printer = m_Module.getLayoutPrinter();
  if (printer)
    printer->recordInputActions(LayoutPrinter::Load, inputFile.getInput());
  ELFSection *S = createDataSection(inputFile);
  ObjectFile *objFile = llvm::cast<ObjectFile>(&inputFile);
  objFile->addSection(S);
  addDescriptionSymbols(inputFile, S);
  return {};
}

ELFSection *BinaryFileParser::createDataSection(InputFile &inputFile) {
  ELFSection *S = m_Module.getScript().sectionMap().createELFSection(
      ".data", LDFileFormat::Regular, llvm::ELF::SHT_PROGBITS,
      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_WRITE, /*EntSize=*/0);
  S->setInputFile(&inputFile);
  // Binary format file data must not be garbage-collected.
  SectionMap &sectionMap = m_Module.getScript().sectionMap();
  sectionMap.addEntrySection(S);

  // Create and add fragment
  llvm::StringRef buf = inputFile.getContents();
  Fragment *F =
      make<RegionFragment>(buf, S, Fragment::Type::Region, /*Align=*/8);
  S->addFragmentAndUpdateSize(F);
  return S;
}

std::string BinaryFileParser::getSymPrefix(const InputFile &inputFile) const {
  std::string s =
      "_binary_" + inputFile.getInput()->decoratedPath(/*showAbsolute=*/false);
  for (char &c : s)
    if (!llvm::isAlnum(c))
      c = '_';
  return s;
}

void BinaryFileParser::addDescriptionSymbols(InputFile &inputFile,
                                             ELFSection *S) {
  IRBuilder &builder = *m_Module.getIRBuilder();
  std::string symPrefix = getSymPrefix(inputFile);
  LDSymbol *startSym = builder.AddSymbol(
      inputFile, symPrefix + "_start", ResolveInfo::Type::Object,
      ResolveInfo::Define, ResolveInfo::Binding::Global,
      /*size=*/0, /*value=*/0, S, ResolveInfo::Visibility::Default,
      /*isPostLTOPhase=*/0, /*idx=*/0, /*idx=*/0);
  LDSymbol *endSym = builder.AddSymbol(
      inputFile, symPrefix + "_end", ResolveInfo::Type::Object,
      ResolveInfo::Define, ResolveInfo::Binding::Global,
      /*size=*/0, /*value=*/S->size(), S, ResolveInfo::Visibility::Default,
      /*isPostLTOPhase=*/0, /*idx=*/1, /*idx=*/0);
  LDSymbol *sizeSym = builder.AddSymbol(
      inputFile, symPrefix + "_size", ResolveInfo::Type::Object,
      ResolveInfo::Define, ResolveInfo::Binding::Absolute, /*size=*/0,
      /*value=*/S->size(), S, ResolveInfo::Visibility::Default,
      /*isPostLTOPhase=*/0, /*idx=*/2, /*idx=*/llvm::ELF::SHN_ABS);

  // Binary format input file symbols shouldn't be garbage-collected!
  startSym->setShouldIgnore(false);
  endSym->setShouldIgnore(false);
  sizeSym->setShouldIgnore(false);
}
