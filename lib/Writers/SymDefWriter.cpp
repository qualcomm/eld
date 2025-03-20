//===- SymDefWriter.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Writers/SymDefWriter.h"
#include "eld/Core/Module.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;
using namespace eld;

SymDefWriter::SymDefWriter(LinkerConfig &PConfig)
    : Config(PConfig), SymDefFile(nullptr) {}

eld::Expected<void> SymDefWriter::init() {
  std::error_code Error;
  if (!Config.options().symDefFile().empty()) {
    SymDefFile = new llvm::raw_fd_ostream(Config.options().symDefFile().c_str(),
                                          Error, llvm::sys::fs::OF_None);
    if (Error && Config.options().symDefFile().c_str()[0] != '\0') {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(
              Diag::unable_to_write_symdef,
              {Config.options().symDefFile().c_str(), Error.message()}));
    }
  }
  return {};
}

SymDefWriter::~SymDefWriter() {
  if (SymDefFile) {
    SymDefFile->flush();
    delete SymDefFile;
  }
}

llvm::raw_ostream &SymDefWriter::outputStream() const {
  if (SymDefFile) {
    SymDefFile->flush();
    return *SymDefFile;
  }
  return llvm::errs();
}

void SymDefWriter::addHeader() {
  outputStream() << "#<"
                 << "SYMDEFS";
  if (Config.isSymDefStyleValid() && !Config.isSymDefStyleDefault())
    outputStream() << "-" << Config.getSymDefString();
  outputStream() << ">#"
                 << "\n";
  outputStream() << "#DO NOT EDIT#"
                 << "\n";
}

std::error_code SymDefWriter::writeSymDef(Module &CurModule) {
  addHeader();
  std::vector<ResolveInfo *> &Symbols = CurModule.getSymbols();
  for (auto &S : Symbols) {
    if (S->isDyn())
      continue;
    if (S->isAbsolute())
      continue;
    if (S->isLocal())
      continue;
    if ((S->type() == ResolveInfo::Section) || (S->type() == ResolveInfo::File))
      continue;
    uint32_t Type = S->type();
    StringRef TypeAsStr;
    switch (Type) {
    case ResolveInfo::NoType:
      TypeAsStr = "NOTYPE";
      break;
    case ResolveInfo::Function:
      TypeAsStr = "FUNC";
      break;
    case ResolveInfo::Object:
      TypeAsStr = "OBJECT";
      break;
    default:
      continue;
    }
    outputStream() << "0x";
    outputStream().write_hex(S->outSymbol()->value());
    outputStream() << "\t";
    outputStream() << TypeAsStr;
    outputStream() << "\t";
    outputStream() << S->name();
    outputStream() << "\n";
  }
  return std::error_code();
}
