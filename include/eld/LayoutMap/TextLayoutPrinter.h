//===- TextLayoutPrinter.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_TEXTLAYOUTPRINTER_H
#define ELD_LAYOUTMAP_TEXTLAYOUTPRINTER_H
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/Input.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MemoryRegion.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class LinkerConfig;

class TextLayoutPrinter {
public:
  TextLayoutPrinter(LayoutInfo *ThisLayoutInfo);

  eld::Expected<void> init();

  llvm::raw_ostream &outputStream() const;

  void printInputActions(bool UseColor = false);

  void printLinkerInsertedTimingStats(Module &CurModule);

  void printBuildStatistics(Module &CurModule, bool UseColor = false);

  void printArchiveRecords(Module &Module, bool UseColor = false);

  void printScriptIncludes(bool UseColor = false);

  void printVersionScripts(bool UseColor = false);

  void printExternList(Module &CurModule, bool UseColor);

  void printCommons(eld::Module &CurModule, bool UseColor = false);

  void printFrag(eld::Module &CurModule, ELFSection *Section, Fragment *Frag,
                 bool Color = false);

  void printSection(GNULDBackend const &Backend,
                    const OutputSectionEntry *Section, bool UseColor);

  void printArchAndVersion(bool UseColor, Module const &M);

  void printFragInfo(Fragment *F, LayoutFragmentInfo *Info, ELFSection *Section,
                     Module &M) const;

  void printPadding(ELFSection *Sec, int64_t StartOffset, int64_t Sz,
                    int64_t FillValue, bool IsAlign,
                    bool UseColor = false) const;

  void printMapFile(eld::Module &Module);

  void printLayout(eld::Module &Module);

  void printGlobalPluginInfo(eld::Module &M, bool UseColor);

  void printPluginInfo(eld::Module &M);

  virtual ~TextLayoutPrinter();

  void clearInputRecords();

  void addLayoutMessage(std::string Msg) {
    outputStream() << "\n\n=======================================\n";
    outputStream() << Msg << "\n";
    outputStream() << "=======================================\n";
  }

  /// Prints rule-matching info in the map file. This function must only be
  /// called before section-merging.
  void printRuleMatchingInfo(Module &Module);

  void flush();

private:
  void printChangeOutputSectionInfo(const ELFSection *S) const;

  void printChunkOps(eld::Module &M, Fragment *F) const;

  bool printChangeOutputSectionPluginOpDetailed(PluginOp *);

  bool printChunkOps(PluginOp *Op) const;

  bool printResetOffsetPluginOp(PluginOp *Op) const;

  bool printAddChunkPluginOp(PluginOp *Pop) const;

  bool printRemoveChunkPluginOp(PluginOp *Pop) const;

  bool printRemoveSymbolPluginOp(PluginOp *Pop) const;

  bool printUpdateChunksPluginOp(PluginOp *Pop) const;

  bool printUpdateRulePluginOp(PluginOp *Pop) const;

  bool printRelocationDataPluginOp(eld::Module &M, PluginOp *Pop) const;

  void printOnlyLayoutFrag(eld::Module &CurModule, ELFSection *Section,
                           Fragment *Frag, bool Color = false);

  void printOnlyLayoutSection(GNULDBackend const &Backend,
                              const OutputSectionEntry *Section, bool UseColor);

  void printOnlyLayoutPadding(ELFSection *Sec, int64_t StartOffset, int64_t Sz,
                              int64_t FillValue, bool IsAlign,
                              bool UseColor = false) const;

  void printStats(LayoutInfo::Stats &L, const Module &Module);

  void printStat(llvm::StringRef S, uint64_t Stats);

  void printPluginStats(
      const std::vector<std::pair<std::string, std::string>> &Stats,
      Plugin *P) const;

  void printStat(llvm::StringRef S, const std::string &Stat) const;

  std::optional<std::string>
  getPluginSectionAnnotations(const ELFSection *S) const;

  std::string showDecoratedSymbolName(eld::Module &CurModule,
                                      const ResolveInfo *R) const;

  std::string commandLineWithMappings();

  void printSegments(GNULDBackend const &Backend, const OutputSectionEntry *OS);

  void printMemoryRegions(GNULDBackend const &Backend,
                          const OutputSectionEntry *OS);

  void printMergeString(MergeableString *S, Module &M) const;

  void printIsFileHeaderLoadedInfo(bool IsLoaded, bool UseColor);

  void printIsPHDRSLoadedInfo(bool IsLoaded, bool UseColor);

  void printScriptCommands(const LinkerScript &Script);

  void printMemoryCommand(const MemoryCmd *Cmd);

  void printTotalSymbolStats(const Module &Module);

  void printDiscardedSymbolStats(const Module &Module);

  void printDynamicList(Module &CurModule, bool UseColor);

  void printVersionList(Module &CurModule, bool UseColor);

  std::string getDecoratedPath(const Input *I) const;

  void printFragments(Module &Module, ELFSection &OutSect, RuleContainer &R,
                      bool UseColor);

  void printSymbolResolution(Module &Module);

  void printOffsetHelper(bool HasOffset, std::function<void()> F) const;

private:
  std::string Storage;
  std::unique_ptr<llvm::raw_string_ostream> Buffer = nullptr;
  std::unique_ptr<llvm::raw_fd_ostream> LayoutFile = nullptr;
  LayoutInfo *ThisLayoutInfo = nullptr;
};

} // namespace eld

#endif
