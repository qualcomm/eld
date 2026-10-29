//===- SearchDirs.h--------------------------------------------------------===//
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

#ifndef ELD_INPUT_SEARCHDIRS_H
#define ELD_INPUT_SEARCHDIRS_H
#include "eld/Input/Input.h"
#include "eld/Support/Path.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include <string>
#include <vector>

namespace eld {
class DiagnosticEngine;
class LinkerConfig;
class ELDFile;
class ELDDirectory;

/** \class SearchDirs
 *  \brief SearchDirs contains the list of paths that MCLinker will search for
 *  archive libraries and control scripts.
 *
 *  SearchDirs is customized for linking. It handles -L on the command line
 *  and SEARCH_DIR macro in the link script.
 *
 *  @see ELDDirectory.
 */
class SearchDirs {
  static const std::string MainExecutablePath;

public:
  typedef std::vector<ELDDirectory *> DirListType;
  using DirRange = llvm::iterator_range<DirListType::iterator>;

  /// Input type preference for library search.
  enum SearchInputType {
    Archive,
    DynObj,
    Script,
  };

  struct SearchResult {
    /// Path without the chroot applied. It should be used for linker script
    /// rule-matching and diagnostics where feasible.
    sys::fs::Path logicalPath;
    /// Path after applying chroot. It should be used to actually read the
    /// input.
    sys::fs::Path resolvedPath;
  };

public:
  SearchDirs(DiagnosticEngine *Diag) : DiagEngine(Diag) {}

  SearchDirs(DiagnosticEngine *Diag, const sys::fs::Path &PSysRoot)
      : SysRoot(PSysRoot), DiagEngine(Diag) {}

  ~SearchDirs() {}

  // find - give a namespec, return logical and chroot-resolved paths.
  const SearchResult *find(const std::string &PNamespec,
                           SearchInputType PPreferType) const;

  const eld::sys::fs::Path *findLibrary(llvm::StringRef Type,
                                        std::string LibraryName,
                                        const LinkerConfig &Config) const;

  /// Look up a file in -L Dirs and return the first match found or nullptr
  const eld::sys::fs::Path *findFile(llvm::StringRef Type,
                                     const std::string &FileName,
                                     const std::string &PluginName) const;

  const eld::sys::fs::Path *findInDirList(llvm::StringRef Type,
                                          const std::string &FileName,
                                          const std::string &PluginName) const;
  const eld::sys::fs::Path *
  findInDefaultConfigPath(llvm::StringRef Type, llvm::StringRef ConfigName,
                          llvm::StringRef PluginName) const;

  const eld::sys::fs::Path *findInRPath(llvm::StringRef Type,
                                        llvm::StringRef LibraryName,
                                        llvm::StringRef RPath) const;

  const eld::sys::fs::Path *findInPath(llvm::StringRef Type,
                                       llvm::StringRef LibraryName) const;

  // FIXME: Unused function.
  const eld::sys::fs::Path *findInCurDir(llvm::StringRef LibraryName) const;

  void setSysRoot(const sys::fs::Path &PSysRoot) { SysRoot = PSysRoot; }
  const sys::fs::Path &sysroot() const { return SysRoot; }
  bool hasSysRoot() const { return !SysRoot.empty(); }

  void setChroot(const sys::fs::Path &PChroot) { Chroot = PChroot; }

  const sys::fs::Path &chroot() const { return Chroot; }

  bool hasChroot() const { return !Chroot.empty(); }

  std::string applyChroot(llvm::StringRef Path) const;

  // -----  iterator range  ----- //
  DirRange getDirectories() {
    return llvm::make_range(DirList.begin(), DirList.end());
  }
  llvm::iterator_range<DirListType::const_iterator> getDirectories() const {
    return llvm::make_range(DirList.begin(), DirList.end());
  }

  // -----  modifiers  ----- //
  bool insert(const char *PDirectory);

  bool insert(const std::string &PDirectory);

  bool insert(const sys::fs::Path &PDirectory);

  const DirListType &getDirList() const { return DirList; }

  // Support loading of default plugins
  std::vector<eld::sys::fs::Path> getDefaultPluginConfigs() const;

private:
  bool checkInputFile(SearchInputType Type, llvm::StringRef NameSpec,
                      llvm::StringRef FileName) const;

  bool checkLibraryOrConfigFile(llvm::StringRef Type,
                                llvm::StringRef LibraryName,
                                llvm::StringRef FileName,
                                llvm::StringRef Using) const;

  DirListType DirList;
  sys::fs::Path SysRoot;
  sys::fs::Path Chroot;
  DiagnosticEngine *DiagEngine;
};

} // namespace eld

#endif
