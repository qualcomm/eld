//===- LinkStatsReport.cpp-------------------------------------------------===//
// Part of the eld Project, under the BSD License.
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/LayoutMap/LinkStatsReport.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InputTree.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace eld;

static llvm::StringRef getInputFileType(InputFile::InputFileKind Kind) {
  switch (Kind) {
  case InputFile::ELFObjFileKind:
    return "Object";
  case InputFile::ELFDynObjFileKind:
    return "SharedObject";
  case InputFile::ELFExecutableFileKind:
    return "Executable";
  case InputFile::BitcodeFileKind:
    return "Bitcode";
  case InputFile::GNUArchiveFileKind:
    return "Archive";
  case InputFile::GNULinkerScriptKind:
    return "LinkerScript";
  case InputFile::ELFSymDefFileKind:
    return "SymDef";
  case InputFile::ELFProvideSymDefFileKind:
    return "ProvideSymDef";
  case InputFile::InternalInputKind:
    return "Internal";
  case InputFile::BinaryFileKind:
    return "Binary";
  case InputFile::UnsupportedKind:
    return "Unsupported";
  case InputFile::UnknownKind:
    return "Unknown";
  }
  llvm_unreachable("Unhandled input file kind");
}

bool eld::emitLinkStatsReport(Module &M, llvm::StringRef Filename,
                              DiagnosticEngine *DiagEngine) {
  LayoutInfo *Layout = M.getLayoutInfo();
  if (!Layout)
    return false;

  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC);
  if (EC) {
    if (DiagEngine)
      DiagEngine->raise(Diag::unable_to_write_json_file)
          << Filename << EC.message();
    return false;
  }

  llvm::json::Array InputFiles;
  InputBuilder &Builder = M.getIRBuilder()->getInputBuilder();
  for (Node *InputNode : Builder.getInputs()) {
    auto *File = llvm::dyn_cast<FileNode>(InputNode);
    if (!File)
      continue;
    Input *Inp = File->getInput();
    InputFile *InpFile = Inp->getInputFile();
    if (!InpFile)
      continue;

    std::string Path = Layout->getPath(Inp);
    InputFiles.push_back(llvm::json::Object{
        {"File", llvm::sys::path::filename(Path).str()},
        {"Type", getInputFileType(InpFile->getKind()).str()}});
  }

  llvm::json::Object Root;
  Root["InputFiles"] = std::move(InputFiles);
  OS << llvm::formatv("{0:2}\n", llvm::json::Value(std::move(Root)));
  return true;
}
