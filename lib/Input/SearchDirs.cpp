//===- SearchDirs.cpp------------------------------------------------------===//
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

//===- SearchDirs.cpp -----------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Input/SearchDirs.h"
#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/ELDDirectory.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/Path.h"
#include "eld/Support/StringUtils.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"

using namespace eld;

const std::string SearchDirs::MainExecutablePath =
    std::string(llvm::sys::path::parent_path(
        llvm::sys::fs::getMainExecutable(nullptr, nullptr)));

namespace {

constexpr inline llvm::StringRef foundString(bool Found) {
  return Found ? "found" : "not found";
}

bool checkInputFile(Input::InputType Type, llvm::StringRef NameSpec,
                    llvm::StringRef FileName, DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(Diag::verbose_trying_input_file)
      << FileName << Input::toString(Type) << NameSpec << foundString(Found);
  return Found;
}

bool checkLibraryOrConfigFile(llvm::StringRef Type, llvm::StringRef LibraryName,
                              llvm::StringRef FileName, llvm::StringRef Using,
                              DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(Diag::verbose_trying_library)
      << FileName << Type << LibraryName << Using << foundString(Found);
  return Found;
}

} // namespace

//===----------------------------------------------------------------------===//
// Non-member functions
//===----------------------------------------------------------------------===//
static inline bool SpecToFilename(const std::string &pSpec, std::string &pFile,
                                  uint32_t pType) {
  bool StartsWithColon = (pSpec.c_str()[0] == ':');
  pFile.clear();
  if (StartsWithColon) {
    pFile = pSpec.substr(1, pSpec.length());
    return true;
  }
  if (pType == Input::Script) {
    pFile = pSpec;
    return false;
  } else {
    pFile = "lib";
    pFile += pSpec;
    switch (pType) {
    case Input::DynObj:
      pFile += ".so";
      break;
    case Input::Archive:
      pFile += ".a";
      break;
    default:
      break;
    }
    return false;
  }
}

//===----------------------------------------------------------------------===//
// SearchDirs
//===----------------------------------------------------------------------===//
bool SearchDirs::insert(const std::string &PPath) {
  auto *Dir = make<ELDDirectory>(PPath, SysRoot.native());
  if (!llvm::sys::fs::is_directory(Dir->name())) {
    Dir->setIsFound(false);
    DirList.push_back(Dir);
    return false;
  }
  Dir->setIsFound(true);
  DirList.push_back(Dir);
  return true;
}

bool SearchDirs::insert(const char *PPath) {
  return insert(std::string(PPath));
}

bool SearchDirs::insert(const sys::fs::Path &PPath) {
  return insert(PPath.native());
}

const eld::sys::fs::Path *SearchDirs::find(const std::string &pNamespec,
                                           Input::InputType pType) const {
  std::string File;
  bool hasNamespace = false;
  hasNamespace = SpecToFilename(pNamespec, File, pType);

  // for all ELDDirectorys
  DirListType::const_iterator EldDir, EldDirEnd = DirList.end();
  for (EldDir = DirList.begin(); EldDir != EldDirEnd; ++EldDir) {
    std::string dirName = (*EldDir)->name().c_str();
    // Need to insert the / in the path between dirName and fileName manually
    std::string fileName = dirName + "/" + File;
    if (checkInputFile(pType, pNamespec, fileName, DiagEngine))
      return make<eld::sys::fs::Path>(fileName);
    if (Input::DynObj == pType && !hasNamespace) {
      // we should also try linking with archives if we dont find a DSO
      SpecToFilename(pNamespec, fileName, Input::Archive);
      fileName = dirName + "/" + fileName;
      if (checkInputFile(Input::Archive, pNamespec, fileName, DiagEngine))
        return make<eld::sys::fs::Path>(fileName);
    }
  } // end of for
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInDirList(llvm::StringRef Type, const std::string &FileName,
                          const std::string &PluginName) const {
  for (const ELDDirectory *Dir : DirList) {
    llvm::SmallString<128> Path(Dir->name());
    llvm::sys::path::append(Path, FileName);
    if (checkLibraryOrConfigFile(Type, FileName, Path, "search path",
                                 DiagEngine))
      return make<eld::sys::fs::Path>(Path.str().str());
  }
  if (PluginName.empty())
    return nullptr;

  const eld::sys::fs::Path *ConfigPath =
      findInDefaultConfigPath(Type, FileName, PluginName);
  if (ConfigPath)
    DiagEngine->raise(Diag::verbose_found_config_file)
        << ConfigPath->getFullPath();
  return ConfigPath;
}

std::vector<eld::sys::fs::Path> SearchDirs::getDefaultPluginConfigs() const {
  std::vector<eld::sys::fs::Path> DefaultPlugins;
  std::string PluginConfigDir = "/../etc/ELD/Plugins/Default/";
  llvm::SmallString<128> Path(SearchDirs::MainExecutablePath);
  llvm::sys::path::append(Path, PluginConfigDir);
  DiagEngine->raise(Diag::verbose_find_default_plugins) << Path.str();
  std::error_code Ec;
  if (!(llvm::sys::fs::exists(Path) && llvm::sys::fs::is_directory(Path)))
    return DefaultPlugins;
  for (llvm::sys::fs::recursive_directory_iterator I(Path, Ec), E; I != E;
       I.increment(Ec)) {
    if (Ec) {
      DiagEngine->raise(Diag::error_finding_default_plugins) << Ec.message();
      return std::vector<eld::sys::fs::Path>();
    }
    std::string Path = I->path();
    if (llvm::sys::fs::is_directory(Path))
      continue;
    DefaultPlugins.push_back(eld::sys::fs::Path(Path));
  }
  // Create a deterministic order to make it easy to reproduce failures
  // if any
  std::sort(DefaultPlugins.begin(), DefaultPlugins.end(),
            [](auto &A, auto &B) { return A.native() < B.native(); });
  return DefaultPlugins;
}

const eld::sys::fs::Path *
SearchDirs::findFile(llvm::StringRef Type, const std::string &Name,
                     const std::string &PluginName) const {
  if (checkLibraryOrConfigFile(Type, Name, Name, "file path", DiagEngine))
    return make<eld::sys::fs::Path>(Name);
  return findInDirList(Type, Name, PluginName);
}

const eld::sys::fs::Path *
SearchDirs::findLibrary(llvm::StringRef Type, std::string LibraryName,
                        const LinkerConfig &Config) const {
  bool HasHash = false;
  if (Config.hasMappingForFile(LibraryName)) {
    LibraryName = Config.getHashFromFile(LibraryName);
    HasHash = true;
  }
  if (checkLibraryOrConfigFile(Type, LibraryName, LibraryName, "file path",
                               DiagEngine))
    return make<eld::sys::fs::Path>(LibraryName);
  // We should not try to find libraries at other places if mapping file is
  // provided and it contains the hash for the LibraryName parameter.
  if (HasHash) {
    Config.raise(Diag::fatal_mapped_file_not_found)
        << LibraryName << Config.getFileFromHash(LibraryName);
    return nullptr;
  }
  std::string LibName = llvm::sys::path::filename(LibraryName).str();
  if (const eld::sys::fs::Path *Path = findInDirList(Type, LibName, ""))
    return Path;
  if (const eld::sys::fs::Path *Path = findInRPath(Type, LibName, RPATH))
    return Path;
  return findInPath(Type, LibName);
}

const eld::sys::fs::Path *SearchDirs::findInRPath(llvm::StringRef Type,
                                                  llvm::StringRef LibraryName,
                                                  llvm::StringRef RPath) const {
#ifdef _WIN32
  const std::string fileName =
      SearchDirs::MainExecutablePath + "/" + LibraryName.str();
  if (checkLibraryOrConfigFile(Type, LibraryName, fileName, "rpath",
                               DiagEngine))
    return make<eld::sys::fs::Path>(fileName);
#else
  llvm::SmallVector<llvm::StringRef, 8> RPathSplit;
  RPath.split(RPathSplit, ":", -1, false);
  for (auto R : RPathSplit) {
    std::string CRPath = R.str();
    eld::string::ReplaceString(CRPath, "$ORIGIN",
                               SearchDirs::MainExecutablePath);
    const std::string FileName = CRPath + "/" + LibraryName.str();
    if (checkLibraryOrConfigFile(Type, LibraryName, FileName, "rpath",
                                 DiagEngine))
      return make<eld::sys::fs::Path>(FileName);
  }
#endif
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInDefaultConfigPath(llvm::StringRef Type,
                                    llvm::StringRef ConfigName,
                                    llvm::StringRef PluginName) const {
  DiagEngine->raise(Diag::note_using_default_config_path);
  std::string PluginConfigDir = "/../etc/ELD/Plugins/";
  llvm::SmallString<128> Path(SearchDirs::MainExecutablePath);
  llvm::sys::path::append(Path, PluginConfigDir);
  llvm::sys::path::append(Path, PluginName);
  llvm::sys::path::append(Path, ConfigName);
  if (checkLibraryOrConfigFile(Type, ConfigName, Path, "search path",
                               DiagEngine))
    return make<eld::sys::fs::Path>(Path.str().str());
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInPath(llvm::StringRef Type,
                       llvm::StringRef LibraryName) const {
  std::vector<std::string> PathSplit;
  std::optional<std::string> Path;
#ifdef _WIN32
  Path = llvm::sys::Process::GetEnv("PATH");
  if (!Path)
    return nullptr;
  PathSplit = eld::string::split(*Path, ';');
#else
  Path = llvm::sys::Process::GetEnv("LD_LIBRARY_PATH");
  if (!Path)
    return nullptr;
  PathSplit = eld::string::split(*Path, ':');
#endif
  for (auto R : PathSplit) {
    const std::string FileName = R + "/" + LibraryName.str();
    if (checkLibraryOrConfigFile(Type, LibraryName, FileName, "system path",
                                 DiagEngine))
      return make<eld::sys::fs::Path>(FileName);
  }
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInCurDir(llvm::StringRef LibraryName) const {
  llvm::SmallString<128> PWD;
  llvm::sys::fs::current_path(PWD);
  if (PWD.empty())
    return nullptr;
  const std::string FileName = std::string(PWD) + "/" + LibraryName.str();
  if (llvm::sys::fs::exists(FileName))
    return make<eld::sys::fs::Path>(FileName);
  return nullptr;
}
