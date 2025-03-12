//===- ObjectLinker.h------------------------------------------------------===//
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
#ifndef ELD_OBJECT_OBJECTLINKER_H
#define ELD_OBJECT_OBJECTLINKER_H
#include "eld/Core/Module.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Support/MappingFile.h"
#include "eld/Support/Path.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/FileOutputBuffer.h"
#include <memory>
#include <vector>

namespace llvm {
namespace lto {
class LTO;
struct Config;
} // namespace lto
} // namespace llvm

namespace eld::plugin {
class LinkerWrapper;
class LinkerPlugin;
} // namespace eld::plugin
namespace eld {

class ArchiveReader;
class BinaryFileParser;
class BinaryWriter;
class BitcodeFile;
class BitcodeReader;
class DynObjWriter;
class ELFSection;
class ELFExecutableFileReader;
class ExecWriter;
class GNULDBackend;
class GroupReader;
class Input;
class InputTree;
class IRBuilder;
class LinkerConfig;
class Module;
class ArchiveParser;
class ELFDynObjParser;
class ELFRelocObjParser;
class ObjectReader;
class Relocation;
class ResolveInfo;
class ScriptReader;

/** \class ObjectLinker
 */
class ObjectLinker {
public:
  ObjectLinker(LinkerConfig &pConfig, GNULDBackend &pLDBackend);

  bool initialize(Module &pModule, eld::IRBuilder &pBuilder);

  void close();

  /// initStdSections - initialize standard sections of the output file.
  bool initStdSections();

  /// normalize - normalize the input files
  bool normalize();

  // process the input file.
  bool readAndProcessInput(Input *input, bool isPostLTO);

  void parseDynList();

  bool parseVersionScript();

  /// linkable - check the linkability of current LinkerConfig
  ///  Check list:
  ///  - check the Attributes are not violate the constaint
  ///  - check every Input has a correct Attribute
  bool linkable() const;

  /// readRelocations - read all relocation entries
  bool readRelocations();

  /// merge strings and handle relocations
  void doMergeStrings();

  /// dataStrippingOpt - optimizations for reducing code size
  void dataStrippingOpt();

  // Run garbage collection in phase `Phase'
  void runGarbageCollection(const std::string &Phase,
                            bool CommonSectionsOnly = false);

  /// mergeSections - put allinput sections into output sections
  bool mergeSections();

  /// addSymbolsToOutput - after all symbols has been resolved, add the symbol
  /// to output
  bool addSymbolsToOutput();

  /// Allocates common symbols. More precisely, create a section and
  /// a fragment for each common symbol.
  ///
  /// Each common symbol is allocated to a unique internal input section
  /// of the name 'COMMON.${CommonSymbolName}'. No common symbols are
  /// allocated to the same internal input section. All common internal
  /// input sections are part of 'CommonSymbols' internal input file.
  ///
  /// \note Common symbol are only allocated for partial links if
  /// define common symbols option is specified.
  /// \note Common symbols are not allocated for bitcode common
  /// symbols.
  bool allocateCommonSymbols();

  bool addDynamicSymbols();

  /// addUndefSymbols - add any symbols specified by the -u flag
  /// forced undefined symbols
  ///   @return true if symbols added
  bool addUndefSymbols();

  /// addDuplicateCodeInsteadOfTrampolines - code for symbols that can be
  /// duplicated instead of trampolines.
  void addDuplicateCodeInsteadOfTrampolines();

  /// add function to not reuse trampolines, instead create trampolines
  /// when jumping to a particular symbol
  void addNoReuseOfTrampolines();

  /// addStandardSymbols - shared object and executable files need some
  /// standard symbols
  ///   @return if there are some input symbols with the same name to the
  ///   standard symbols, return false
  bool addStandardSymbols();
  bool addSectionSymbols();

  /// addTargetSymbols - some targets, such as MIPS and ARM, need some
  /// target-dependent symbols
  ///   @return if there are some input symbols with the same name to the
  ///   target symbols, return false
  bool addTargetSymbols();

  /// addScriptSymbols - define symbols from the command line option or linker
  /// scripts.
  bool addScriptSymbols();

  /// addDynListSymbols - add the symbols in dynamic list supplied in
  /// dynamic-list argument.
  bool addDynListSymbols();

  /// scanRelocations - scan all relocation entries by output symbols.
  bool scanRelocations(bool isPartialLink = false);

  void scanRelocationsHelper(InputFile *input, bool isPartialLink,
                             LinkerScript::PluginVectorT PluginVect,
                             Relocator::CopyRelocs &);

  bool finalizeScanRelocations();

  /// initStubs - initialize stub-related stuff.
  bool initStubs();

  /// prelayout - help backend to do some modification before layout
  bool prelayout();

  // Create relocation sections for emit relocs.
  void createRelocationSections();

  /// layout - linearly layout all output sections and reserve some space
  /// for GOT/PLT
  ///   Because we do not support instruction relaxing in this early version,
  ///   if there is a branch can not jump to its target, we return false
  ///   directly
  bool layout();

  /// postlayout - help backend to do some modification after layout
  bool postlayout();

  /// postlayout - help backend to do some modification after layout
  bool printlayout();

  bool finalizeBeforeWrite();

  /// relocate - applying relocation entries and create relocation
  /// section in the output files
  /// Create relocation section, asking GNULDBackend to
  /// read the relocation information into RelocationEntry
  /// and push_back into the relocation section
  bool relocation(bool emitRelocs);

  /// finalizeSymbolValue - finalize the symbol value for symbols
  /// originating from object files.
  bool finalizeSymbolValues();

  /// finalizeSymbolValue - finalize the symbol value
  void finalizeSymbolValue(ResolveInfo *I) const;

  /// emitOutput - emit the output file.
  bool emitOutput(llvm::FileOutputBuffer &pOutput);

  /// postProcessing - do modificatiion after all processes
  eld::Expected<void> postProcessing(llvm::FileOutputBuffer &pOutput);

  // -----  readers and writers  ----- //
  ELFRelocObjParser *getNewRelocObjParser() const {
    return m_NewRelocObjParser;
  }

  ELFExecObjParser *getELFExecObjParser() const { return m_ELFExecObjParser; }

  BinaryFileParser *getBinaryFileParser() const { return m_BinaryFileParser; }

  ELFDynObjParser *getNewDynObjReader() const { return m_NewDynObjReader; }

  ArchiveParser *getArchiveParser() const { return m_ArchiveParser; }

  GroupReader *getGroupReader() const { return m_pGroupReader; }

  ScriptReader *getScriptReader() const { return m_pScriptReader; }

  BitcodeReader *getBitcodeReader() const { return m_pBitcodeReader; }

  ObjectReader *getSymDefReader() const { return m_pSymDefReader; }

  ELFObjectWriter *getWriter() const { return m_pWriter; }

  /// LTO specific methods
  void setLTOPlugin(plugin::LinkerPlugin &LTOPlugin);
  bool createLTOObject();
  bool runAssembler(std::vector<std::string> &files, std::string RelocModel,
                    const std::vector<std::string> &asmFilesFromLTO);
  void beginPostLTO();

  const GNULDBackend &getTargetBackend() const { return m_LDBackend; }
  GNULDBackend &getTargetBackend() { return m_LDBackend; }

  void assignOutputSections(std::vector<eld::InputFile *> &inputs);

  // Sync Relocations.
  void syncRelocations(uint8_t *buffer);

  void syncRelocationResult(uint8_t *data, InputFile *input);

  bool mergeInputSections(ObjectBuilder &builder,
                          std::vector<Section *> &sections);

  bool mayBeSortSections(std::vector<Section *> &sections);

  bool createOutputSection(ObjectBuilder &builder, OutputSectionEntry *output,
                           bool postLayout = false);

  // For sections inside ELFFileFormat
  void markDiscardFileFormatSections();

  void assignOffset(OutputSectionEntry *);

  void assignOffset(ELFSection *);

  // Assigns offsets to 'GROUP' sections.
  void assignOffsetToGroupSections();

  // -------------------Support for sorting--------------------------------
  void sortByName(RuleContainer *I, bool SortRule);

  void sortByAlignment(RuleContainer *I, bool SortRule);

  void sortByInitPriority(RuleContainer *I, bool SortRule);

  bool sortSections(RuleContainer *I, bool SortRule);

  // -------------------LinkerScript Support--------------------------------
  bool readLinkerScript(InputFile *I);

  bool readInputs(const std::vector<Node *> &N);

  bool getInputs(std::vector<InputFile *> &inputs);

  // ---------------------SectionIterator Plugin Support ------------------
  bool runSectionIteratorPlugin();

  // ---------------------Embedded BC Support ----------------------------
  bool overrideELFObjectWithBitCode(InputFile *I);

  // -------------------Plugin Support -----------------------------------

  bool initializeMerge();

  bool updateInputSectionMappingsForPlugin();

  bool finishAssignOutputSections();

  /// Update section mapping for pending section overrides associated with the
  /// LinkerWrapper LW. If LW is nullptr, then update section mapping for all
  /// the pending section overrides.
  bool finishAssignOutputSections(const plugin::LinkerWrapper *LW);

  bool runOutputSectionIteratorPlugin();

  bool initializeOutputSectionsAndRunPlugin();

  // Get Plugin list for Relocation registration callback.
  LinkerScript::PluginVectorT getLinkerPluginWithLinkerConfigs();

  // ------------------ All sections -------------------------
  const std::vector<Section *> &getAllInputSections() const {
    return AllInputSections;
  }

  void addInputSection(Section *inputSection) {
    AllInputSections.push_back(inputSection);
  }

  // -------------------------Backend input processing -------------
  bool processInputFiles();

  // -------------------------Backend symbols support --------------
  bool provideGlobalSymbolAndContents(std::string Name, size_t Sz,
                                      uint32_t Alignment);

  // --------------------Export LTO Phase --------------------------
  bool isPostLTOPhase() const { return m_postLTOPhase; }

  struct SymbolStats {
    uint64_t global = 0;
    uint64_t local = 0;
    uint64_t weak = 0;
    uint64_t hidden = 0;
    uint64_t absolute = 0;
    uint64_t protectedSyms = 0;
    uint64_t file = 0;
  };

  const SymbolStats &getTotalSymbolStats() const;

  const SymbolStats &getDiscardedSymbolStats() const;

  /// Stores all the entry sections in SectionMap.
  void collectEntrySections() const;

  const ArchiveFile *
  getArchiveFileFromMemoryAreaToAFMap(const MemoryArea *memArea) const {
    auto iter = m_MemoryAreaToArchiveFileMap.find(memArea);
    if (iter == m_MemoryAreaToArchiveFileMap.end())
      return nullptr;
    return iter->second;
  }

  void addToMemoryAreaToAFMap(const ArchiveFile &AF) {
    const MemoryArea *memArea = AF.getInput()->getMemArea();
    m_MemoryAreaToArchiveFileMap[memArea] = &AF;
  }

private:
  std::unique_ptr<llvm::lto::LTO> LTOInit(llvm::lto::Config Conf);

  bool
  FinalizeLTOSymbolResolution(llvm::lto::LTO &LTO,
                              const std::vector<BitcodeFile *> &bitCodeFiles);

  bool DoLTO(llvm::lto::LTO &LTO);

  /// writeRelocationResult - helper function of syncRelocationResult, write
  /// relocation target data to output
  void writeRelocationResult(Relocation &pReloc, uint8_t *pOutput);

  /// writeRelocationData - another helper to write relocation data into the
  /// output
  void writeRelocationData(Relocation &pReloc, uint64_t Data, uint8_t *pOutput);

  /// addSymbolToOutput - add a symbol to output symbol table if it's not a
  /// section symbol and not defined in the discarded section
  bool addSymbolToOutput(const ResolveInfo &pInfo) const;

  bool insertPostLTOELF();

  size_t getRelocSectSize(uint32_t type, uint32_t count);

  std::pair<std::optional<llvm::Reloc::Model>, std::string>
  ltoCodegenModel() const;

  /// Assign '.bss' output section to all those internal common sections which
  /// are not assigned any output section as per the mapping information
  /// specified in the linker script.
  ///
  /// This is required because linker does not set default mappings when a
  /// linker script is provided. But we do need to assign '.bss' output section
  /// to any common symbol that is left-out by the linker script.
  ///
  /// This function should only be called after output sections are assigned to
  /// input sections using the mapping information specified by the linker
  /// script.
  bool setCommonSectionsFallbackToBSS();
  bool setCopyRelocSectionsFallbackToBSS();

  // ------------------ exclude-lto-filelist, include-lto-filelist --------
  bool parseIncludeOrExcludeLTOfiles();

  bool DoVerifyPlugin(plugin::Plugin *P);

  bool mayBeAddSectionSymbol(ELFSection *S);

  std::string getLTOTempPrefix() const;

  llvm::Expected<std::unique_ptr<llvm::raw_fd_ostream>>
  createLTOTempFile(size_t Task, const std::string &Suffix,
                    llvm::SmallString<256> &FileName) const;

  /// Adds input files to outputTar if --reproduce option is used
  void addInputFileToTar(InputFile *ipt, MappingFile::Kind K);

  void addInputFileToTar(const std::string &name, MappingFile::Kind kind);

  void addLTOOutputToTar();

  void addTempFilesToTar(size_t maxTasks);

  // ---------------------extern-list, other extern list files parsing----------
  bool parseListFile(std::string File, uint32_t Type);

  void createCopyRelocation(ResolveInfo &Sym, Relocation::Type Type);

  void accountSymForSymStats(SymbolStats &symbolStats, const ResolveInfo &RI);

  void accountSymForTotalSymStats(const ResolveInfo &RI);

  void accountSymForDiscardedSymStats(const ResolveInfo &RI);

  // ---------------------String merging  --------------------- //
  void mergeIdenticalStrings() const;

  void mergeNonAllocStrings(std::vector<OutputSectionEntry *>,
                            ObjectBuilder &Builder) const;

  /// Redirect relocations to point directly to a deduplicated string fragment
  void fixMergeStringRelocations() const;

  void reportPendingPluginRuleInsertions() const;

  /// Finalizes the output section flags.
  ///
  /// This function must be called after updating the output section flags
  /// using each input section it contained.
  void finalizeOutputSectionFlags(OutputSectionEntry *OSE) const;

private:
  LinkerConfig &m_Config;
  Module *m_pModule;
  eld::IRBuilder *m_pBuilder;

  GNULDBackend &m_LDBackend;

  // -----  readers and writers  ----- //
  ELFRelocObjParser *m_NewRelocObjParser = nullptr;
  ELFDynObjParser *m_NewDynObjReader = nullptr;
  ArchiveParser *m_ArchiveParser = nullptr;
  ELFExecObjParser *m_ELFExecObjParser = nullptr;
  BinaryFileParser *m_BinaryFileParser = nullptr;
  GroupReader *m_pGroupReader;
  ScriptReader *m_pScriptReader;
  ELFObjectWriter *m_pWriter;
  BitcodeReader *m_pBitcodeReader;
  ObjectReader *m_pSymDefReader;
  llvm::StringSet<> m_DynlistExports;

  // Is this the second phase of normalize for LTO
  bool m_postLTOPhase;

  // Set to true once any GC pass has run. Used to know if shouldIgnore() on
  // a symbol is meaningful.
  bool m_GCHasRun = false;

  bool m_SaveTemps;

  bool m_TraceLTO;

  std::string m_LTOTempPrefix;

  // Paths of all generated LTO objects
  std::vector<std::string> ltoObjects;

  // Paths of other temporary files, that need to be cleaned up.
  std::vector<std::string> filesToRemove;

  std::vector<Input *> LTOELFFiles;

  std::vector<WildcardPattern *> LTOPatternList;

  plugin::LinkerPlugin *LTOPlugin = nullptr;

  std::vector<Section *> AllInputSections;

  std::mutex Mutex;

  SymbolStats m_TotalSymStats;

  SymbolStats m_DiscardedSymStats;

  /// It is used to reuse ArchiveFileInfo when an archive is read multiple
  /// times. ArchiveFileInfo is obtained as 'archiveFile->getArchiveFileInfo()'.
  std::unordered_map<const MemoryArea *, const ArchiveFile *>
      m_MemoryAreaToArchiveFileMap;
};

} // end namespace eld
#endif
