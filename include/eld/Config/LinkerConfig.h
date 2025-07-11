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
} // namespace ELD

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

  explicit LinkerConfig(DiagnosticEngine *DiagEngine,
                        const std::string &PTripleString);

  ~LinkerConfig();

  typedef std::vector<CommandLine *> CommandLineVectorT;

  const GeneralOptions &options() const { return GenOptions; }
  GeneralOptions &options() { return GenOptions; }

  const TargetOptions &targets() const { return Targets; }
  TargetOptions &targets() { return Targets; }

  CodeGenType codeGenType() const { return CodeGen; }

  void setCodeGenType(CodeGenType PType) { CodeGen = PType; }

  CodePosition codePosition() const { return CodePos; }
  void setCodePosition(CodePosition PPosition) { CodePos = PPosition; }

  bool isCodeIndep() const { return (Independent == CodePos); }
  bool isCodeDynamic() const {
    return ((CodeGen == DynObj) || (DynamicDependent == CodePos));
  }

  bool isBuildingExecutable() const {
    return (CodeGen == Exec || GenOptions.isPIE());
  }

  bool isLinkPartial() const { return CodeGen == LinkerConfig::Object; }

  bool isCodeStatic() const { return (StaticDependent == CodePos); }

  static const char *version();

  void printOptions(llvm::raw_ostream &, GNULDBackend const &,
                    bool UseColor = false);

  bool isAssignOutputSectionsMultiThreaded() const {
    return EnableThreads & LinkerConfig::AssignOutputSections;
  }

  bool isScanRelocationsMultiThreaded() const {
    return EnableThreads & LinkerConfig::ScanRelocations;
  }

  bool isSyncRelocationsMultiThreaded() const {
    return EnableThreads & LinkerConfig::SyncRelocations;
  }

  bool isCheckCrossRefsMultiThreaded() const {
    return EnableThreads & LinkerConfig::CheckCrossRefs;
  }

  bool isCreateOutputSectionsMultiThreaded() const {
    return EnableThreads & LinkerConfig::CreateOutputSections;
  }

  bool isApplyRelocationsMultiThreaded() const {
    return EnableThreads & LinkerConfig::ApplyRelocations;
  }

  bool isLinkerRelaxationMultiThreaded() const {
    return EnableThreads & LinkerConfig::LinkerRelaxation;
  }

  void setThreadOptions(uint32_t EnableThreadsOpt) {
    EnableThreads = NoThreads;
    if (EnableThreadsOpt & AssignOutputSections)
      EnableThreads |= AssignOutputSections;
    if (EnableThreadsOpt & ScanRelocations)
      EnableThreads |= ScanRelocations;
    if (EnableThreadsOpt & SyncRelocations)
      EnableThreads |= SyncRelocations;
    if (EnableThreadsOpt & CheckCrossRefs)
      EnableThreads |= CheckCrossRefs;
    if (EnableThreadsOpt & CreateOutputSections)
      EnableThreads |= CreateOutputSections;
    if (EnableThreadsOpt & ApplyRelocations)
      EnableThreads |= ApplyRelocations;
    if (EnableThreadsOpt & LinkerRelaxation)
      EnableThreads |= LinkerRelaxation;
  }

  void disableThreadOptions(uint32_t ThreadOptions) {
    EnableThreads &= ~ThreadOptions;
  }

  bool isGlobalThreadingEnabled() const { return GlobalThreadingEnabled; }

  void setGlobalThreadingEnabled() {
    EnableThreads = AllThreads;
    GlobalThreadingEnabled = true;
  }

  void addCommandLine(llvm::StringRef Option, bool Flag);

  void addCommandLine(llvm::StringRef Option, const char *);

  void addCommandLine(llvm::StringRef Option,
                      const std::vector<std::string> &Args);

  void addCommandLine(llvm::StringRef Option, llvm::StringRef Args);

  CommandLineVectorT getCommandLineVectorT() { return CommandLineVector; }

  /// ---------------------Mapping file functionality---------------------------
  void addMapping(const std::string Name, const std::string Hash) {
    HashToPath[Hash] = Name;
    PathToHash[Name] = Hash;
  }

  std::string getFileFromHash(const std::string &Hash) const;

  std::string getHashFromFile(const std::string &FileName) const;

  bool hasMappingForFile(const std::string &FileName) const {
    return PathToHash.find(FileName) != PathToHash.end();
  }

  bool hasMappingForHash(const std::string &Hash) const {
    return PathToHash.find(Hash) != HashToPath.end();
  }

  /// Returns the path to the file that maps to the thin archive member as
  /// per the provided mapping file.
  std::string getMappedThinArchiveMember(const std::string &ArchiveName,
                                         const std::string &MemberName) const;

  DiagnosticPrinter *getPrinter() const { return DiagEngine->getPrinter(); }

  DiagnosticEngine *getDiagEngine() const { return DiagEngine; }

  /// Raise diagnostics.
  MsgHandler raise(unsigned int PId) const;

  /// Raise diagnostic using a DiagnosticEntry.
  void raiseDiagEntry(std::unique_ptr<plugin::DiagnosticEntry> DiagEntry) const;

  /// search directory
  const SearchDirs &directories() const { return SearchDirs; }

  SearchDirs &directories() { return SearchDirs; }

  void setSysRoot(std::string SysRoot) { SearchDirs.setSysRoot(SysRoot); }

  const SearchDirs &searchDirs() const { return SearchDirs; }

  /// ---------------------Wall functionality---------------------------
  bool hasShowAllWarnings() const {
    return WarnOpt.EnableAllWarnings.has_value();
  }

  bool hasShowLinkerScriptWarnings() const {
    return WarnOpt.EnableLinkerScriptWarnings.has_value();
  }

  bool hasShowZeroSizedSectionsWarnings() const {
    return WarnOpt.EnableZeroSizedSectionsWarnings.has_value();
  }

  bool hasCommandLineWarnings() const {
    return WarnOpt.EnableCommandLineWarnings.has_value();
  }

  bool hasShowAttributeMixWarnings() const {
    return WarnOpt.EnableAttributeMixWarnings.has_value();
  }

  bool hasShowArchiveFileWarnings() const {
    return WarnOpt.EnableArchiveFileWarnings.has_value();
  }

  bool hasShowLinkerScriptMemoryWarnings() const {
    return WarnOpt.EnableLinkerScriptMemoryWarnings.has_value();
  }

  bool hasBadDotAssignmentsWarnings() const {
    return WarnOpt.EnableBadDotAssignmentWarnings.has_value();
  }

  bool hasWholeArchiveWarnings() const {
    return WarnOpt.EnableWholeArchiveWarnings.has_value();
  }

  bool showAllWarnings() const {
    return (hasShowAllWarnings() && *WarnOpt.EnableAllWarnings);
  }

  bool showLinkerScriptWarnings() const {
    return (hasShowLinkerScriptWarnings() &&
            *WarnOpt.EnableLinkerScriptWarnings);
  }

  bool showZeroSizedSectionsWarnings() const {
    return (hasShowZeroSizedSectionsWarnings() &&
            *WarnOpt.EnableZeroSizedSectionsWarnings);
  }

  bool showCommandLineWarnings() const {
    return (hasCommandLineWarnings() && *WarnOpt.EnableCommandLineWarnings);
  }

  bool showAttributeMixWarnings() const {
    return (hasShowAttributeMixWarnings() &&
            *WarnOpt.EnableAttributeMixWarnings);
  }

  bool showArchiveFileWarnings() const {
    return (hasShowArchiveFileWarnings() && *WarnOpt.EnableArchiveFileWarnings);
  }

  bool showLinkerScriptMemoryWarnings() const {
    return (hasShowLinkerScriptMemoryWarnings() &&
            *WarnOpt.EnableLinkerScriptMemoryWarnings);
  }

  bool showBadDotAssignmentWarnings() const {
    return (hasBadDotAssignmentsWarnings() &&
            *WarnOpt.EnableBadDotAssignmentWarnings);
  }

  bool showWholeArchiveWarnings() const {
    return (hasWholeArchiveWarnings() && *WarnOpt.EnableWholeArchiveWarnings);
  }

  void setShowAllWarnings() {
    WarnOpt.EnableAllWarnings = true;
    WarnOpt.EnableLinkerScriptWarnings = true;
    WarnOpt.EnableZeroSizedSectionsWarnings = true;
    WarnOpt.EnableAttributeMixWarnings = true;
    WarnOpt.EnableArchiveFileWarnings = true;
    WarnOpt.EnableLinkerScriptMemoryWarnings = true;
    WarnOpt.EnableBadDotAssignmentWarnings = true;
    WarnOpt.EnableWholeArchiveWarnings = true;
  }

  void setShowLinkerScriptWarning(bool Option) {
    WarnOpt.EnableLinkerScriptWarnings = Option;
  }

  void setShowCommandLineWarning(bool Option) {
    WarnOpt.EnableCommandLineWarnings = Option;
  }

  void setShowZeroSizedSectionsWarning(bool Option) {
    WarnOpt.EnableZeroSizedSectionsWarnings = Option;
  }

  void setShowAttributeMixWarning(bool Option) {
    WarnOpt.EnableAttributeMixWarnings = Option;
  }

  void setShowArchiveFileWarning(bool Option) {
    WarnOpt.EnableArchiveFileWarnings = Option;
  }

  void setShowLinkerScriptMemoryWarning(bool Option) {
    WarnOpt.EnableLinkerScriptMemoryWarnings = Option;
  }

  void setShowBadDotAssginmentsWarning(bool Option) {
    WarnOpt.EnableBadDotAssignmentWarnings = Option;
  }

  void setShowWholeArchiveWarning(bool Option) {
    WarnOpt.EnableWholeArchiveWarnings = Option;
  }

  bool setWarningOption(llvm::StringRef WarnOpt);

  /// Returns true if UseOldStyleTrampolineNames contains any value.
  bool hasUseOldStyleTrampolineName() const {
    return UseOldStyleTrampolineNames.has_value();
  }

  /// Returns true if old trampoline naming style must be used.
  bool useOldStyleTrampolineName() const {
    return hasUseOldStyleTrampolineName() && *UseOldStyleTrampolineNames;
  }

  /// Set the value for use old style for trampoline naming convention.
  void setUseOldStyleTrampolineName(bool B) { UseOldStyleTrampolineNames = B; }

  /// Returns true if reproduce tar should be created.
  bool shouldCreateReproduceTar() const;

  void setSymDefStyle(llvm::StringRef Style);

  bool isSymDefStyleValid() const;

  bool isSymDefStyleDefault() const;

  bool isSymDefStyleProvide() const;

  std::string getSymDefString() const;

protected:
  CommandLineVectorT CommandLineVector;

private:
  // -----  General Options  ----- //
  GeneralOptions GenOptions;
  TargetOptions Targets;
  CodeGenType CodeGen;
  CodePosition CodePos;
  SymDefStyle SymDefStyleValue = Default;
  std::unordered_map<std::string, std::string> HashToPath;
  std::unordered_map<std::string, std::string> PathToHash;
  bool GlobalThreadingEnabled = false;
  uint32_t EnableThreads = LinkerConfig::EnableThreadsOpt::AllThreads;
  DiagnosticEngine *DiagEngine;
  SearchDirs SearchDirs;
  WarnOptions WarnOpt;
  std::optional<bool> UseOldStyleTrampolineNames;
};

} // namespace eld

#endif
