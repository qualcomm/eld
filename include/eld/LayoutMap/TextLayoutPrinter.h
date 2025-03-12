//===- TextLayoutPrinter.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_TEXTLAYOUTPRINTER_H
#define ELD_LAYOUTMAP_TEXTLAYOUTPRINTER_H
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/Input.h"
#include "eld/LayoutMap/LayoutPrinter.h"
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
  TextLayoutPrinter(LayoutPrinter *Printer);

  eld::Expected<void> init();

  llvm::raw_ostream &outputStream() const;

  void printInputActions(bool useColor = false);

  void printLinkerInsertedTimingStats(Module &pModule);

  void printBuildStatistics(Module &pModule, bool useColor = false);

  void printArchiveRecords(Module &module, bool useColor = false);

  void printScriptIncludes(bool useColor = false);

  void printVersionScripts(bool useColor = false);

  void printExternList(Module &pModule, bool useColor);

  void printCommons(eld::Module &pModule, bool useColor = false);

  void printFrag(eld::Module &pModule, ELFSection *section, Fragment *frag,
                 bool color = false);

  void printSection(GNULDBackend const &backend,
                    const OutputSectionEntry *section, bool useColor);

  void printArchAndVersion(bool useColor, GNULDBackend const &backend);

  void printFragInfo(Fragment *F, LayoutFragmentInfo *Info, ELFSection *Section,
                     Module &M) const;

  void printPadding(ELFSection *sec, int64_t startOffset, int64_t sz,
                    int64_t fillValue, bool isAlign,
                    bool useColor = false) const;

  void printMapFile(eld::Module &module);

  void printLayout(eld::Module &module);

  void printGlobalPluginInfo(eld::Module &M, bool useColor);

  void printPluginInfo(eld::Module &M);

  virtual ~TextLayoutPrinter();

  void destroy();

  void clearInputRecords();

  void addLayoutMessage(std::string pMsg) {
    outputStream() << "\n\n=======================================\n";
    outputStream() << pMsg << "\n";
    outputStream() << "=======================================\n";
  }

  /// Prints rule-matching info in the map file. This function must only be
  /// called before section-merging.
  void printRuleMatchingInfo(Module &module);

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

  bool printRelocationDataPluginOp(eld::Module &M, PluginOp *Pop) const;

  void printOnlyLayoutFrag(eld::Module &pModule, ELFSection *section,
                           Fragment *frag, bool color = false);

  void printOnlyLayoutSection(GNULDBackend const &Backend,
                              const OutputSectionEntry *section, bool useColor);

  void printOnlyLayoutPadding(ELFSection *sec, int64_t startOffset, int64_t sz,
                              int64_t fillValue, bool isAlign,
                              bool useColor = false) const;

  void printStats(LayoutPrinter::Stats &L, const Module &module);

  void printStat(llvm::StringRef S, uint64_t Stats);

  void printStat(llvm::StringRef S, const std::string &stat) const;

  std::string showDecoratedSymbolName(eld::Module &pModule,
                                      const ResolveInfo *R) const;

  std::string commandLineWithMappings();

  void printSegments(GNULDBackend const &Backend, const OutputSectionEntry *OS);

  void printMemoryRegions(GNULDBackend const &Backend,
                          const OutputSectionEntry *OS);

  void printMergeString(MergeableString *S, Module &M) const;

  void printIsFileHeaderLoadedInfo(bool isLoaded, bool useColor);

  void printIsPHDRSLoadedInfo(bool isLoaded, bool useColor);

  void printScriptCommands(const LinkerScript &script);

  void printMemoryCommand(const MemoryCmd *Cmd);

  void printTotalSymbolStats(const Module &module);

  void printDiscardedSymbolStats(const Module &module);

  void printDynamicList(Module &pModule, bool useColor);

  void printVersionList(Module &pModule, bool useColor);

  std::string getDecoratedPath(const Input *I) const;

  void printFragments(Module &module, ELFSection &outSect, RuleContainer &R,
                      bool useColor);

  void printSymbolResolution(Module &module);

private:
  std::string Storage;
  std::unique_ptr<llvm::raw_string_ostream> Buffer = nullptr;
  std::unique_ptr<llvm::raw_fd_ostream> layoutFile = nullptr;
  LayoutPrinter *m_LayoutPrinter = nullptr;
};

} // namespace eld

#endif
