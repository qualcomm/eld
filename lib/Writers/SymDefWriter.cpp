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

SymDefWriter::SymDefWriter(LinkerConfig &pConfig)
    : m_Config(pConfig), m_SymDefFile(nullptr) {}

eld::Expected<void> SymDefWriter::init() {
  std::error_code error;
  if (!m_Config.options().symDefFile().empty()) {
    m_SymDefFile = new llvm::raw_fd_ostream(
        m_Config.options().symDefFile().c_str(), error, llvm::sys::fs::OF_None);
    if (error && m_Config.options().symDefFile().c_str()[0] != '\0') {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::FatalDiagnosticEntry(
              diag::unable_to_write_symdef,
              {m_Config.options().symDefFile().c_str(), error.message()}));
    }
  }
  return {};
}

SymDefWriter::~SymDefWriter() {
  if (m_SymDefFile) {
    m_SymDefFile->flush();
    delete m_SymDefFile;
  }
}

llvm::raw_ostream &SymDefWriter::outputStream() const {
  if (m_SymDefFile) {
    m_SymDefFile->flush();
    return *m_SymDefFile;
  }
  return llvm::errs();
}

void SymDefWriter::addHeader() {
  outputStream() << "#<"
                 << "SYMDEFS";
  if (m_Config.isSymDefStyleValid() && !m_Config.isSymDefStyleDefault())
    outputStream() << "-" << m_Config.getSymDefString();
  outputStream() << ">#"
                 << "\n";
  outputStream() << "#DO NOT EDIT#"
                 << "\n";
}

std::error_code SymDefWriter::writeSymDef(Module &pModule) {
  addHeader();
  std::vector<ResolveInfo *> &symbols = pModule.getSymbols();
  for (auto &S : symbols) {
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
