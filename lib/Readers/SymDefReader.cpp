//===- SymDefReader.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/SymDefReader.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/Input.h"
#include "eld/LayoutMap/TextLayoutPrinter.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Object/SectionMap.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/NamePool.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/FileSystem.h"
#include <string>
#include <utility>

using namespace eld;
using namespace llvm;

/// constructor
SymDefReader::SymDefReader(GNULDBackend &pBackend, eld::IRBuilder &pBuilder,
                           LinkerConfig &pConfig)
    : ObjectReader(), m_Builder(pBuilder), m_Config(pConfig) {}

SymDefReader::~SymDefReader() {}

bool SymDefReader::readSymbols(InputFile &pFile, bool isPostLTOPhase) {
  std::vector<std::tuple<uint64_t, ResolveInfo::Type, std::string>>
      symDefVector = readSymDefs(pFile);
  if (!symDefVector.size())
    return false;
  for (auto &symDef : symDefVector) {
    uint64_t symVal = std::get<0>(symDef);
    ResolveInfo::Type resolverType = std::get<1>(symDef);
    std::string symName = std::get<2>(symDef);
    if (m_Builder.getModule().getPrinter()->traceSymDef())
      m_Config.raise(Diag::note_read_from_symdef_file)
          << symName << pFile.getInput()->decoratedPath();
    if (m_Config.isSymDefStyleProvide())
      m_Builder.getModule().getBackend()->addSymDefProvideSymbol(
          symName, resolverType, symVal, &pFile);
    else {
      m_Builder.addSymbol<IRBuilder::Force, IRBuilder::Resolve>(
          &pFile, symName, resolverType, ResolveInfo::Define,
          ResolveInfo::Absolute,
          0x0,                 // size
          symVal,              // value
          FragmentRef::null(), // FragRef
          ResolveInfo::Default,
          false,  // isPostLTOPhase
          false); // isBitCode
      if (m_Builder.getModule().getPrinter()->traceSymDef())
        m_Config.raise(Diag::note_resolving_from_symdef_file)
            << symName << pFile.getInput()->decoratedPath();
    }
  }

  return true;
}

bool SymDefReader::readHeader(InputFile &pFile, bool isPostLTOPhase) {
  (void)(m_Config);
  (void)(m_Builder);
  return true;
}

void SymDefReader::getSymDefStyle(llvm::StringRef line) {
  std::pair<StringRef, StringRef> lineAndRest = line.split('-');
  lineAndRest = lineAndRest.second.split('>');
  m_Config.setSymDefStyle(lineAndRest.first);
}

void SymDefReader::processComment(llvm::StringRef line) {
  if (line.contains("#<SYMDEFS"))
    getSymDefStyle(line);
}

std::vector<std::tuple<uint64_t, ResolveInfo::Type, std::string>>
SymDefReader::readSymDefs(InputFile &pFile) {
  StringRef buffer = pFile.getContents();
  std::vector<std::tuple<uint64_t, ResolveInfo::Type, std::string>>
      SymDefVector;
  std::vector<std::tuple<uint64_t, ResolveInfo::Type, std::string>>
      EmptySymDefVector;

  while (!buffer.empty()) {
    // Split off each line in the file.
    std::pair<StringRef, StringRef> lineAndRest = buffer.split('\n');
    StringRef line = lineAndRest.first.trim();
    // Comment lines starts with # or ;
    if (line.empty() || line.starts_with("#") || line.starts_with(";")) {
      processComment(line);
      buffer = lineAndRest.second;
      continue;
    }
    buffer = lineAndRest.first.trim();
    SmallVector<StringRef, 3> SymDefs;
    buffer.split(SymDefs, "\t");
    uint64_t symVal = 0;
    if (SymDefs.size() != 3)
      return EmptySymDefVector;
    // Read the symbol value.
    if (SymDefs[0].trim().getAsInteger(0, symVal))
      return EmptySymDefVector;
    ResolveInfo::Type Type =
        llvm::StringSwitch<ResolveInfo::Type>(SymDefs[1].trim())
            .Case("NOTYPE", ResolveInfo::NoType)
            .Case("OBJECT", ResolveInfo::Object)
            .Case("FUNC", ResolveInfo::Function)
            .Default(ResolveInfo::CommonBlock);
    if (Type == ResolveInfo::CommonBlock)
      return EmptySymDefVector;
    SymDefVector.push_back(std::make_tuple(symVal, Type, SymDefs[2].str()));
    buffer = lineAndRest.second;
  }
  return SymDefVector;
}

bool SymDefReader::readSections(InputFile &pFile, bool isPostLTOPhase) {
  return true;
}

bool SymDefReader::readRelocations(InputFile &pFile) { return true; }
