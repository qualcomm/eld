//===- LinkerConfig.h------------------------------------------------------===//
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
#ifndef ELD_CONFIG_LINKERCONFIG_H
#define ELD_CONFIG_LINKERCONFIG_H

#include "eld/Config/GeneralOptions.h"
#include "eld/Config/TargetOptions.h"
#include "eld/Core/CommandLine.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Diagnostics/MsgHandler.h"
#include "eld/Input/SearchDirs.h"
#include "eld/Support/Path.h"
#include "llvm/TargetParser/Triple.h"
#include <optional>
#include <string>
#include <vector>

namespace ELD {
class DiagnosticEntry;
}

namespace eld {
class DiagnosticEngine;
class SearchDirs;
class LinkerConfig {
public:
  enum CodeGenType { Unknown, Object, DynObj, Exec, External, Binary };

  /** \enum CodePosition
   *  CodePosition indicates the ability of the generated output to be
   *  loaded at different addresses. If the output can be loaded at different
   *  addresses, we say the output is position independent. Shared libraries
   *  and position-independent executable programs (PIE) are in this category.
   *  ::Independent indicates the output is position independent.
   *  If a executable program can not be loaded at arbitrary addresses, but it
   *  can call outside functions, we say the program is dynamic dependent on
   *  the address to be loaded. ::DynamicDependent indicates the output is not
   *  only a executable program, but also dynamic dependent. In general,
   *  executable programs are dynamic dependent.
   *  If a executable program can not be loaded at different addresses, and
   *  only call inner functions, then we say the program is static dependent on
   *  its loaded address. ::StaticDependent is used to indicate this kind of
   *  output.
   */
  enum CodePosition {
    Independent,      ///< Position Independent
    DynamicDependent, ///< Can call outside libraries
    StaticDependent,  ///< Can not call outside libraries
    Unset             ///< Undetermine code position mode
  };

  enum EnableThreadsOpt {
    NoThreads = 0,
    AssignOutputSections = 0x1,
    ScanRelocations = 0x2,
    SyncRelocations = 0x4,
    CheckCrossRefs = 0x8,
    CreateOutputSections = 0x10,
    ApplyRelocations = 0x20,
    LinkerRelaxation = 0x40,
    AllThreads = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80,
  };

  enum SymDefStyle { Default, Provide, UnknownSymDefStyle };

  struct WarnOptions {
    std::optional<bool> EnableAllWarnings;
    std::optional<bool> EnableLinkerScriptWarnings;
    std::optional<bool> EnableZeroSizedSectionsWarnings;
    std::optional<bool> EnableAttributeMixWarnings;
    std::optional<bool> EnableArchiveFileWarnings;
    std::optional<bool> EnableLinkerScriptMemoryWarnings;
    std::optional<bool> EnableBadDotAssignmentWarnings;
    std::optional<bool> EnableWholeArchiveWarnings;
    std::optional<bool> EnableCommandLineWarnings;
  };

public:
  LinkerConfig(DiagnosticEngine *);

  explicit LinkerConfig(DiagnosticEngine *diagEngine,
                        const std::string &pTripleString);

  ~LinkerConfig();

  typedef std::vector<CommandLine *> CommandLineVectorT;

  const GeneralOptions &options() const { return m_Options; }
  GeneralOptions &options() { return m_Options; }

  const TargetOptions &targets() const { return m_Targets; }
  TargetOptions &targets() { return m_Targets; }

  CodeGenType codeGenType() const { return m_CodeGenType; }

  void setCodeGenType(CodeGenType pType) { m_CodeGenType = pType; }

  CodePosition codePosition() const { return m_CodePosition; }
  void setCodePosition(CodePosition pPosition) { m_CodePosition = pPosition; }

  bool isCodeIndep() const { return (Independent == m_CodePosition); }
  bool isCodeDynamic() const {
    return ((m_CodeGenType == DynObj) || (DynamicDependent == m_CodePosition));
  }

  bool isBuildingExecutable() const {
    return (m_CodeGenType == Exec || m_Options.isPIE());
  }

  bool isLinkPartial() const { return m_CodeGenType == LinkerConfig::Object; }

  bool isCodeStatic() const { return (StaticDependent == m_CodePosition); }

  static const char *version();

  void printOptions(llvm::raw_ostream &, GNULDBackend const &,
                    bool useColor = false);

  bool isAssignOutputSectionsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::AssignOutputSections;
  }

  bool isScanRelocationsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::ScanRelocations;
  }

  bool isSyncRelocationsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::SyncRelocations;
  }

  bool isCheckCrossRefsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::CheckCrossRefs;
  }

  bool isCreateOutputSectionsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::CreateOutputSections;
  }

  bool isApplyRelocationsMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::ApplyRelocations;
  }

  bool isLinkerRelaxationMultiThreaded() const {
    return m_EnableThreads & LinkerConfig::LinkerRelaxation;
  }

  void setThreadOptions(uint32_t EnableThreadsOpt) {
    m_EnableThreads = NoThreads;
    if (EnableThreadsOpt & AssignOutputSections)
      m_EnableThreads |= AssignOutputSections;
    if (EnableThreadsOpt & ScanRelocations)
      m_EnableThreads |= ScanRelocations;
    if (EnableThreadsOpt & SyncRelocations)
      m_EnableThreads |= SyncRelocations;
    if (EnableThreadsOpt & CheckCrossRefs)
      m_EnableThreads |= CheckCrossRefs;
    if (EnableThreadsOpt & CreateOutputSections)
      m_EnableThreads |= CreateOutputSections;
    if (EnableThreadsOpt & ApplyRelocations)
      m_EnableThreads |= ApplyRelocations;
    if (EnableThreadsOpt & LinkerRelaxation)
      m_EnableThreads |= LinkerRelaxation;
  }

  void disableThreadOptions(uint32_t threadOptions) {
    m_EnableThreads &= ~threadOptions;
  }

  bool isGlobalThreadingEnabled() const { return m_GlobalThreadingEnabled; }

  void setGlobalThreadingEnabled() {
    m_EnableThreads = AllThreads;
    m_GlobalThreadingEnabled = true;
  }

  void addCommandLine(llvm::StringRef option, bool flag);

  void addCommandLine(llvm::StringRef option, const char *);

  void addCommandLine(llvm::StringRef option,
                      const std::vector<std::string> &args);

  void addCommandLine(llvm::StringRef option, llvm::StringRef args);

  CommandLineVectorT getCommandLineVectorT() { return m_CommandLineVector; }

  /// ---------------------Mapping file functionality---------------------------
  void addMapping(const std::string Name, const std::string Hash) {
    m_HashToPath[Hash] = Name;
    m_PathToHash[Name] = Hash;
  }

  // FIXME: parameter name is confusing here. Why the parameter name is filename
  // in the function named 'getFileFromHash'. Shouldn't the parameter name be
  // 'hash'?
  // FIXME: const in return type is redundant and can inhibit optimizations.
  const std::string getFileFromHash(const std::string &FileName) const;

  // FIXME: parameter name is confusing here. Why the parameter name is hash in
  // the function named 'getHashFromFile'. Shouldn't the parameter name be
  // 'filename' here?
  // FIXME: const in return type is redundant and can inhibit optimizations.
  const std::string getHashFromFile(const std::string &Hash) const;

  bool hasMappingForFile(const std::string &FileName) const {
    return m_PathToHash.find(FileName) != m_PathToHash.end();
  }

  bool hasMappingForHash(const std::string &Hash) const {
    return m_PathToHash.find(Hash) != m_HashToPath.end();
  }

  /// Returns the path to the file that maps to the thin archive member as
  /// per the provided mapping file.
  std::string getMappedThinArchiveMember(const std::string &archiveName,
                                         const std::string &memberName) const;

  DiagnosticPrinter *getPrinter() const { return m_DiagEngine->getPrinter(); }

  DiagnosticEngine *getDiagEngine() const { return m_DiagEngine; }

  /// Raise diagnostics.
  MsgHandler raise(unsigned int pID) const;

  /// Raise diagnostic using a DiagnosticEntry.
  void raiseDiagEntry(std::unique_ptr<plugin::DiagnosticEntry> diagEntry) const;

  /// search directory
  const SearchDirs &directories() const { return m_SearchDirs; }

  SearchDirs &directories() { return m_SearchDirs; }

  void setSysRoot(std::string sysRoot) { m_SearchDirs.setSysRoot(sysRoot); }

  const SearchDirs &searchDirs() const { return m_SearchDirs; }

  /// ---------------------Wall functionality---------------------------
  bool hasShowAllWarnings() const {
    return m_WarnOptions.EnableAllWarnings.has_value();
  }

  bool hasShowLinkerScriptWarnings() const {
    return m_WarnOptions.EnableLinkerScriptWarnings.has_value();
  }

  bool hasShowZeroSizedSectionsWarnings() const {
    return m_WarnOptions.EnableZeroSizedSectionsWarnings.has_value();
  }

  bool hasCommandLineWarnings() const {
    return m_WarnOptions.EnableCommandLineWarnings.has_value();
  }

  bool hasShowAttributeMixWarnings() const {
    return m_WarnOptions.EnableAttributeMixWarnings.has_value();
  }

  bool hasShowArchiveFileWarnings() const {
    return m_WarnOptions.EnableArchiveFileWarnings.has_value();
  }

  bool hasShowLinkerScriptMemoryWarnings() const {
    return m_WarnOptions.EnableLinkerScriptMemoryWarnings.has_value();
  }

  bool hasBadDotAssignmentsWarnings() const {
    return m_WarnOptions.EnableBadDotAssignmentWarnings.has_value();
  }

  bool hasWholeArchiveWarnings() const {
    return m_WarnOptions.EnableWholeArchiveWarnings.has_value();
  }

  bool showAllWarnings() const {
    return (hasShowAllWarnings() && *m_WarnOptions.EnableAllWarnings);
  }

  bool showLinkerScriptWarnings() const {
    return (hasShowLinkerScriptWarnings() &&
            *m_WarnOptions.EnableLinkerScriptWarnings);
  }

  bool showZeroSizedSectionsWarnings() const {
    return (hasShowZeroSizedSectionsWarnings() &&
            *m_WarnOptions.EnableZeroSizedSectionsWarnings);
  }

  bool showCommandLineWarnings() const {
    return (hasCommandLineWarnings() &&
            *m_WarnOptions.EnableCommandLineWarnings);
  }

  bool showAttributeMixWarnings() const {
    return (hasShowAttributeMixWarnings() &&
            *m_WarnOptions.EnableAttributeMixWarnings);
  }

  bool showArchiveFileWarnings() const {
    return (hasShowArchiveFileWarnings() &&
            *m_WarnOptions.EnableArchiveFileWarnings);
  }

  bool showLinkerScriptMemoryWarnings() const {
    return (hasShowLinkerScriptMemoryWarnings() &&
            *m_WarnOptions.EnableLinkerScriptMemoryWarnings);
  }

  bool showBadDotAssignmentWarnings() const {
    return (hasBadDotAssignmentsWarnings() &&
            *m_WarnOptions.EnableBadDotAssignmentWarnings);
  }

  bool showWholeArchiveWarnings() const {
    return (hasWholeArchiveWarnings() &&
            *m_WarnOptions.EnableWholeArchiveWarnings);
  }

  void setShowAllWarnings() {
    m_WarnOptions.EnableAllWarnings = true;
    m_WarnOptions.EnableLinkerScriptWarnings = true;
    m_WarnOptions.EnableZeroSizedSectionsWarnings = true;
    m_WarnOptions.EnableAttributeMixWarnings = true;
    m_WarnOptions.EnableArchiveFileWarnings = true;
    m_WarnOptions.EnableLinkerScriptMemoryWarnings = true;
    m_WarnOptions.EnableBadDotAssignmentWarnings = true;
    m_WarnOptions.EnableWholeArchiveWarnings = true;
  }

  void setShowLinkerScriptWarning(bool option) {
    m_WarnOptions.EnableLinkerScriptWarnings = option;
  }

  void setShowCommandLineWarning(bool option) {
    m_WarnOptions.EnableCommandLineWarnings = option;
  }

  void setShowZeroSizedSectionsWarning(bool option) {
    m_WarnOptions.EnableZeroSizedSectionsWarnings = option;
  }

  void setShowAttributeMixWarning(bool option) {
    m_WarnOptions.EnableAttributeMixWarnings = option;
  }

  void setShowArchiveFileWarning(bool option) {
    m_WarnOptions.EnableArchiveFileWarnings = option;
  }

  void setShowLinkerScriptMemoryWarning(bool option) {
    m_WarnOptions.EnableLinkerScriptMemoryWarnings = option;
  }

  void setShowBadDotAssginmentsWarning(bool option) {
    m_WarnOptions.EnableBadDotAssignmentWarnings = option;
  }

  void setShowWholeArchiveWarning(bool option) {
    m_WarnOptions.EnableWholeArchiveWarnings = option;
  }

  bool setWarningOption(llvm::StringRef warnOpt);

  /// Returns true if m_UseOldStyleTrampolineNames contains any value.
  bool hasUseOldStyleTrampolineName() const {
    return m_UseOldStyleTrampolineNames.has_value();
  }

  /// Returns true if old trampoline naming style must be used.
  bool useOldStyleTrampolineName() const {
    return hasUseOldStyleTrampolineName() && *m_UseOldStyleTrampolineNames;
  }

  /// Set the value for use old style for trampoline naming convention.
  void setUseOldStyleTrampolineName(bool b) {
    m_UseOldStyleTrampolineNames = b;
  }

  /// Returns true if reproduce tar should be created.
  bool shouldCreateReproduceTar() const;

  void setSymDefStyle(llvm::StringRef style);

  bool isSymDefStyleValid() const;

  bool isSymDefStyleDefault() const;

  bool isSymDefStyleProvide() const;

  std::string getSymDefString() const;

protected:
  CommandLineVectorT m_CommandLineVector;

private:
  // -----  General Options  ----- //
  GeneralOptions m_Options;
  TargetOptions m_Targets;
  CodeGenType m_CodeGenType;
  CodePosition m_CodePosition;
  SymDefStyle m_SymDefStyle = Default;
  std::unordered_map<std::string, std::string> m_HashToPath;
  std::unordered_map<std::string, std::string> m_PathToHash;
  bool m_GlobalThreadingEnabled = false;
  uint32_t m_EnableThreads = LinkerConfig::EnableThreadsOpt::AllThreads;
  DiagnosticEngine *m_DiagEngine;
  SearchDirs m_SearchDirs;
  WarnOptions m_WarnOptions;
  std::optional<bool> m_UseOldStyleTrampolineNames;
};

} // namespace eld

#endif
