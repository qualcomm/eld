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
  static const std::string m_MainExecutablePath;

public:
  typedef std::vector<ELDDirectory *> DirList;
  typedef DirList::iterator iterator;
  typedef DirList::const_iterator const_iterator;

public:
  SearchDirs(DiagnosticEngine *diag) : m_DiagEngine(diag) {}

  SearchDirs(DiagnosticEngine *diag, const sys::fs::Path &pSysRoot)
      : m_SysRoot(pSysRoot), m_DiagEngine(diag) {}

  ~SearchDirs() {}

  // find - give a namespec, return a real path of the shared object.
  const sys::fs::Path *find(const std::string &pNamespec,
                            Input::Type pPreferType) const;

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
  findInDefaultConfigPath(llvm::StringRef type, llvm::StringRef configName,
                          llvm::StringRef PluginName) const;

  const eld::sys::fs::Path *findInRPath(llvm::StringRef Type,
                                        llvm::StringRef LibraryName,
                                        llvm::StringRef RPath) const;

  const eld::sys::fs::Path *findInPath(llvm::StringRef Type,
                                       llvm::StringRef LibraryName) const;

  const eld::sys::fs::Path *findInCurDir(llvm::StringRef LibraryName) const;

  void setSysRoot(const sys::fs::Path &pSysRoot) { m_SysRoot = pSysRoot; }
  const sys::fs::Path &sysroot() const { return m_SysRoot; }
  bool hasSysRoot() const { return !m_SysRoot.empty(); }

  // -----  iterators  ----- //
  const_iterator begin() const { return m_DirList.begin(); }
  iterator begin() { return m_DirList.begin(); }
  const_iterator end() const { return m_DirList.end(); }
  iterator end() { return m_DirList.end(); }

  // -----  modifiers  ----- //
  bool insert(const char *pDirectory);

  bool insert(const std::string &pDirectory);

  bool insert(const sys::fs::Path &pDirectory);

  const DirList &getDirList() const { return m_DirList; }

  // Support loading of default plugins
  std::vector<eld::sys::fs::Path> getDefaultPluginConfigs() const;

private:
  DirList m_DirList;
  sys::fs::Path m_SysRoot;
  DiagnosticEngine *m_DiagEngine;
};

} // namespace eld

#endif
