//===- Input.cpp-----------------------------------------------------------===//
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

#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/InputFile.h"
#include "eld/Input/InputTree.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/INIReader.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/FileSystem.h"
#include <filesystem>

using namespace eld;

static unsigned int Order = 0;

llvm::StringRef Input::toString(Input::InputType Type) {
  switch (Type) {
  case Archive:
    return "archive";
  case DynObj:
    return "dynamic object";
  case Script:
    return "linker script";
  case Namespec:
    return "namespec";
  case ArchiveMember:
    return "archive member";
  case Internal:
    return "internal";
  case Default:
    return "default";
  }
  llvm_unreachable("Input::InputType out of range");
}

//===----------------------------------------------------------------------===//
// eld::Input
//===----------------------------------------------------------------------===//
Input::Input(std::string PName, DiagnosticEngine *DiagEngine,
             Input::InputType PType)
    : FileName(PName), Attr(), InputOrdinal(Order++), ResolvedPathHash(0),
      MemberNameHash(0), Type(PType), DiagEngine(DiagEngine) {}

Input::Input(std::string PName, const Attribute &Attr,
             DiagnosticEngine *DiagEngine, Input::InputType PType)
    : FileName(PName), Attr(Attr), InputOrdinal(Order++), ResolvedPathHash(0),
      MemberNameHash(0), Type(PType), DiagEngine(DiagEngine) {}

bool Input::resolvePathMappingFile(const LinkerConfig &PConfig) {
  ResolvedPath = eld::sys::fs::Path(PConfig.getFileFromHash(FileName));
  std::string ResolvedPathStr = ResolvedPath->native();
  if (!isPathValid(FileName)) {
    return false;
  }
  ResolvedPathHash = computeFilePathHash(ResolvedPathStr);
  MemberNameHash = computeFilePathHash(FileName);
  MemoryArea *InputMem =
      Input::getMemoryAreaForPath(FileName, PConfig.getDiagEngine());
  if (!InputMem)
    InputMem = createMemoryArea(FileName, PConfig.getDiagEngine());
  setMemArea(InputMem);
  // All queries to return the name of the Input return FileName for the main
  // driver.
  Name = FileName;
  return true;
}

bool Input::isPathValid(const std::string &Path) const {
  if (llvm::sys::fs::is_directory(Path)) {
    DiagEngine->raise(Diag::fatal_cannot_read_input_err)
        << Path << "Is a directory";
    return false;
  }
  return true;
}

/// \return True if path able to be resolved, otherwise false
bool Input::resolvePath(const LinkerConfig &PConfig) {
  if (ResolvedPath)
    return true;
  if (PConfig.options().hasMappingFile() && !isInternal())
    return resolvePathMappingFile(PConfig);
  auto &PSearchDirs = PConfig.directories();
  switch (Type) {
  default:
    ResolvedPath = eld::sys::fs::Path(FileName);
    break;
  case Input::Internal:
    ResolvedPath = eld::sys::fs::Path(FileName);
    return true;
  }

  if (Type == Input::Script) {
    if (PSearchDirs.hasSysRoot() &&
        (FileName.size() > 0 && FileName[0] == '/')) {
      ResolvedPath = PSearchDirs.sysroot();
      ResolvedPath->append(FileName);
    }
    if (!llvm::sys::fs::exists(ResolvedPath->native())) {
      const sys::fs::Path *P = PSearchDirs.find(FileName, Input::Script);
      if (P != nullptr)
        ResolvedPath = *P;
    }
  }
  if (Type == Input::Namespec) {
    const sys::fs::Path *NameSpecPath = nullptr;
    if (Attr.isStatic()) {
      // with --static, we must search an archive.
      NameSpecPath = PSearchDirs.find(FileName, Input::Archive);
    } else {
      // otherwise, with --Bdynamic, we can find either an archive or a
      // shared object.
      NameSpecPath = PSearchDirs.find(FileName, Input::DynObj);
    }
    if (nullptr == NameSpecPath) {
      DiagEngine->raise(Diag::err_cannot_find_namespec) << FileName;
      return false;
    }
    ResolvedPath = *NameSpecPath;
  }

  std::string ResolvedPathStr = ResolvedPath->native();
  if (!isPathValid(ResolvedPathStr)) {
    return false;
  }

  ResolvedPathHash = computeFilePathHash(ResolvedPathStr);
  MemberNameHash = computeFilePathHash(FileName);
  MemoryArea *InputMem =
      Input::getMemoryAreaForPath(ResolvedPathStr, PConfig.getDiagEngine());
  if (!InputMem)
    InputMem =
        Input::createMemoryArea(ResolvedPathStr, PConfig.getDiagEngine());
  if (!InputMem)
    return false;
  setMemArea(InputMem);
  // All queries to return the name of the Input return FileName for the main
  // driver.
  Name = FileName;
  return true;
}

void Input::setInputFile(InputFile *Inp) { IF = Inp; }

void Input::overrideInputFile(InputFile *Inp) { IF = Inp; }

llvm::StringRef Input::getFileContents() const {
  // FIXME: The assert should instead be:
  // ASSERT(MemArea, "Missing memory buffer!");
  ASSERT(MemArea && MemArea->size(), "zero sized!");
  return MemArea->getContents();
}

void Input::releaseMemory(bool IsVerbose) {
  if (IsVerbose)
    DiagEngine->raise(Diag::release_file) << decoratedPath();
  IsReleased = true;
}

Input::~Input() {
  if (isArchiveMember())
    return;
  releaseMemory(false);
  IF = nullptr;
}

void Input::addMemberMatchedPattern(const WildcardPattern *W, bool R) {
  MemberPatternMap[W] = R;
}

bool Input::findMemberMatchedPattern(const WildcardPattern *W, bool &Result) {
  auto F = MemberPatternMap.find(W);
  if (F == MemberPatternMap.end())
    return false;
  Result = F->second;
  return true;
}

uint64_t Input::getWildcardPatternSize() {
  return (sizeof(FilePatternMap) * FilePatternMap.size()) +
         (sizeof(MemberPatternMap) * MemberPatternMap.size());
}

void Input::resize(uint32_t N) {
  FilePatternMap.reserve(N);
  MemberPatternMap.reserve(N);
  PatternMapInitialized = true;
}

void Input::clear() {
  PatternMapInitialized = false;
  FilePatternMap.clear();
  MemberPatternMap.clear();
}

std::string Input::getDecoratedRelativePath(const std::string &Basepath) const {
  if (isInternal())
    return decoratedPath(/*showAbsolute=*/false);
  std::error_code Ec;
  std::filesystem::path P =
      std::filesystem::relative(getResolvedPath().getFullPath(), Basepath, Ec);
  if (Ec || !std::filesystem::exists(Basepath)) {
    DiagEngine->raise(Diag::warn_unable_to_compute_relpath)
        << getResolvedPath().native() << Basepath;
    return decoratedPath(/*showAbsolute=*/false);
  }
  return P.string();
}

llvm::hash_code Input::computeFilePathHash(llvm::StringRef FilePath) {
  return llvm::hash_combine(FilePath);
}

std::unordered_map<std::string, MemoryArea *>
    Input::ResolvedPathToMemoryAreaMap;

MemoryArea *Input::getMemoryAreaForPath(const std::string &Filepath,
                                        DiagnosticEngine *DiagEngine) {
  auto Iter = ResolvedPathToMemoryAreaMap.find(Filepath);
  if (Iter == ResolvedPathToMemoryAreaMap.end())
    return nullptr;
  DiagEngine->raise(Diag::verbose_reusing_prev_memory_mapping) << Filepath;
  return Iter->second;
}

MemoryArea *Input::createMemoryArea(const std::string &Filepath,
                                    DiagnosticEngine *DiagEngine) {
  DiagEngine->raise(Diag::verbose_mapping_file_into_memory) << Filepath;
  MemoryArea *InputMem = make<MemoryArea>(Filepath);
  if (!InputMem->Init(DiagEngine))
    return nullptr;
  ResolvedPathToMemoryAreaMap[Filepath] = InputMem;
  return InputMem;
}
