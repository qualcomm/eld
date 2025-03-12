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

const std::string SearchDirs::m_MainExecutablePath =
    std::string(llvm::sys::path::parent_path(
        llvm::sys::fs::getMainExecutable(nullptr, nullptr)));

namespace {

constexpr inline llvm::StringRef foundString(bool found) {
  return found ? "found" : "not found";
}

bool checkInputFile(Input::Type Type, llvm::StringRef NameSpec,
                    llvm::StringRef FileName, DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(diag::verbose_trying_input_file)
      << FileName << Input::toString(Type) << NameSpec << foundString(Found);
  return Found;
}

bool checkLibraryOrConfigFile(llvm::StringRef Type, llvm::StringRef LibraryName,
                              llvm::StringRef FileName, llvm::StringRef Using,
                              DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(diag::verbose_trying_library)
      << FileName << Type << LibraryName << Using << foundString(Found);
  return Found;
}

} // namespace

//===----------------------------------------------------------------------===//
// Non-member functions
//===----------------------------------------------------------------------===//
static inline bool SpecToFilename(const std::string &pSpec, std::string &pFile,
                                  uint32_t pType) {
  bool startsWithColon = (pSpec.c_str()[0] == ':');
  pFile.clear();
  if (startsWithColon) {
    pFile = pSpec.substr(1, pSpec.length());
    return true;
  } else if (pType == Input::Script) {
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
bool SearchDirs::insert(const std::string &pPath) {
  auto dir = make<ELDDirectory>(pPath, m_SysRoot.native());
  if (!llvm::sys::fs::is_directory(dir->name())) {
    dir->setIsFound(false);
    m_DirList.push_back(dir);
    return false;
  }
  dir->setIsFound(true);
  m_DirList.push_back(dir);
  return true;
}

bool SearchDirs::insert(const char *pPath) {
  return insert(std::string(pPath));
}

bool SearchDirs::insert(const sys::fs::Path &pPath) {
  return insert(pPath.native());
}

const eld::sys::fs::Path *SearchDirs::find(const std::string &pNamespec,
                                           Input::Type pType) const {
  std::string file;
  bool hasNamespace = false;
  hasNamespace = SpecToFilename(pNamespec, file, pType);

  // for all ELDDirectorys
  DirList::const_iterator eld_dir, eld_dir_end = m_DirList.end();
  for (eld_dir = m_DirList.begin(); eld_dir != eld_dir_end; ++eld_dir) {
    std::string dirName = (*eld_dir)->name().c_str();
    // Need to insert the / in the path between dirName and fileName manually
    std::string fileName = dirName + "/" + file;
    if (checkInputFile(pType, pNamespec, fileName, m_DiagEngine))
      return make<eld::sys::fs::Path>(fileName);
    else if (Input::DynObj == pType && !hasNamespace) {
      // we should also try linking with archives if we dont find a DSO
      SpecToFilename(pNamespec, fileName, Input::Archive);
      fileName = dirName + "/" + fileName;
      if (checkInputFile(Input::Archive, pNamespec, fileName, m_DiagEngine))
        return make<eld::sys::fs::Path>(fileName);
    }
  } // end of for
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInDirList(llvm::StringRef Type, const std::string &FileName,
                          const std::string &PluginName) const {
  for (const ELDDirectory *Dir : m_DirList) {
    llvm::SmallString<128> Path(Dir->name());
    llvm::sys::path::append(Path, FileName);
    if (checkLibraryOrConfigFile(Type, FileName, Path, "search path",
                                 m_DiagEngine))
      return make<eld::sys::fs::Path>(Path.str().str());
  }
  if (PluginName.empty())
    return nullptr;

  const eld::sys::fs::Path *configPath =
      findInDefaultConfigPath(Type, FileName, PluginName);
  if (configPath)
    m_DiagEngine->raise(diag::verbose_found_config_file)
        << configPath->getFullPath();
  return configPath;
}

std::vector<eld::sys::fs::Path> SearchDirs::getDefaultPluginConfigs() const {
  std::vector<eld::sys::fs::Path> DefaultPlugins;
  std::string pluginConfigDir = "/../etc/ELD/Plugins/Default/";
  llvm::SmallString<128> Path(SearchDirs::m_MainExecutablePath);
  llvm::sys::path::append(Path, pluginConfigDir);
  m_DiagEngine->raise(diag::verbose_find_default_plugins) << Path.str();
  std::error_code ec;
  if (!(llvm::sys::fs::exists(Path) && llvm::sys::fs::is_directory(Path)))
    return DefaultPlugins;
  for (llvm::sys::fs::recursive_directory_iterator i(Path, ec), e; i != e;
       i.increment(ec)) {
    if (ec) {
      m_DiagEngine->raise(diag::error_finding_default_plugins) << ec.message();
      return std::vector<eld::sys::fs::Path>();
    }
    std::string Path = i->path();
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
  if (checkLibraryOrConfigFile(Type, Name, Name, "file path", m_DiagEngine))
    return make<eld::sys::fs::Path>(Name);
  return findInDirList(Type, Name, PluginName);
}

const eld::sys::fs::Path *
SearchDirs::findLibrary(llvm::StringRef Type, std::string LibraryName,
                        const LinkerConfig &Config) const {
  bool hasHash = false;
  if (Config.hasMappingForFile(LibraryName)) {
    LibraryName = Config.getHashFromFile(LibraryName);
    hasHash = true;
  }
  if (checkLibraryOrConfigFile(Type, LibraryName, LibraryName, "file path",
                               m_DiagEngine))
    return make<eld::sys::fs::Path>(LibraryName);
  // We should not try to find libraries at other places if mapping file is
  // provided and it contains the hash for the LibraryName parameter.
  if (hasHash) {
    Config.raise(diag::fatal_mapped_file_not_found)
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
      SearchDirs::m_MainExecutablePath + "/" + LibraryName.str();
  if (checkLibraryOrConfigFile(Type, LibraryName, fileName, "rpath",
                               m_DiagEngine))
    return make<eld::sys::fs::Path>(fileName);
#else
  llvm::SmallVector<llvm::StringRef, 8> RPathSplit;
  RPath.split(RPathSplit, ":", -1, false);
  for (auto R : RPathSplit) {
    std::string CRPath = R.str();
    eld::string::ReplaceString(CRPath, "$ORIGIN",
                               SearchDirs::m_MainExecutablePath);
    const std::string fileName = CRPath + "/" + LibraryName.str();
    if (checkLibraryOrConfigFile(Type, LibraryName, fileName, "rpath",
                                 m_DiagEngine))
      return make<eld::sys::fs::Path>(fileName);
  }
#endif
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInDefaultConfigPath(llvm::StringRef type,
                                    llvm::StringRef configName,
                                    llvm::StringRef PluginName) const {
  m_DiagEngine->raise(diag::note_using_default_config_path);
  std::string pluginConfigDir = "/../etc/ELD/Plugins/";
  llvm::SmallString<128> Path(SearchDirs::m_MainExecutablePath);
  llvm::sys::path::append(Path, pluginConfigDir);
  llvm::sys::path::append(Path, PluginName);
  llvm::sys::path::append(Path, configName);
  if (checkLibraryOrConfigFile(type, configName, Path, "search path",
                               m_DiagEngine))
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
    const std::string fileName = R + "/" + LibraryName.str();
    if (checkLibraryOrConfigFile(Type, LibraryName, fileName, "system path",
                                 m_DiagEngine))
      return make<eld::sys::fs::Path>(fileName);
  }
  return nullptr;
}

const eld::sys::fs::Path *
SearchDirs::findInCurDir(llvm::StringRef LibraryName) const {
  llvm::SmallString<128> PWD;
  llvm::sys::fs::current_path(PWD);
  if (PWD.empty())
    return nullptr;
  const std::string fileName = std::string(PWD) + "/" + LibraryName.str();
  if (llvm::sys::fs::exists(fileName))
    return make<eld::sys::fs::Path>(fileName);
  return nullptr;
}
