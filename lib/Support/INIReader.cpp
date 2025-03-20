//===- INIReader.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Support/INIReader.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/StringUtils.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace eld;

INIReader::Kind INIReader::getKind(llvm::StringRef Line) const {
  Line = Line.trim();
  if (Line.starts_with("#"))
    return INIReader::Comment;
  if (Line.starts_with("[") && Line.ends_with("]"))
    return INIReader::Section;
  // TODO : Check for errors.
  if (Line.contains("="))
    return INIReader::KeyValue;
  return INIReader::None;
}

INIReaderSection *INIReader::addSection(llvm::StringRef L) {
  L = L.drop_front();
  L = L.drop_back();
  return (INISections[L.str()] = new INIReaderSection(L.str()));
}

std::vector<std::pair<std::string, std::string>> INIReaderSection::getItems() {
  std::vector<std::pair<std::string, std::string>> Res;
  Res.assign(section.begin(), section.end());
  return Res;
}

void INIReader::addValues(INIReaderSection *S, llvm::StringRef L) {
  llvm::SmallVector<llvm::StringRef, 0> Values;
  L.split(Values, '=');
  llvm::StringRef Key = Values[0].trim();
  llvm::StringRef Value = Values[1].trim();
  S->addItem(Key.str(), Value.str());
}

eld::Expected<bool> INIReader::readINIFile() {
  std::vector<std::string> Lines;

  if (!((sys::fs::loadFileContents(INIFileName, Lines) == std::error_code()) &&
        Lines.size()))
    return false;

  // Error if non-ascii
  if (llvm::find_if(Lines, [](const std::string &S) -> bool {
        return !eld::string::isASCII(S);
      }) != Lines.end()) {
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::ini_invalid_character, {INIFileName},
                                plugin::DiagnosticEntry::Severity::Error));
  }

  INIReaderSection *Section = nullptr;
  for (auto &L : Lines) {
    llvm::StringRef Line = llvm::StringRef(L).trim();
    const Kind K = getKind(Line);
    if (K == INIReader::Error)
      return false;
    if (K == INIReader::Comment)
      continue;
    if (K == INIReader::Section)
      Section = addSection(Line);
    if (K == INIReader::KeyValue)
      addValues(Section, Line);
  }
  return true;
}
