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
  ObjectLinker(LinkerConfig &PConfig, GNULDBackend &PLdBackend);

  bool initialize(Module &PModule, eld::IRBuilder &PBuilder);

  void close();

  /// initStdSections - initialize standard sections of the output file.
  bool initStdSections();

  /// normalize - normalize the input files
  bool normalize();

  // process the input file.
  bool readAndProcessInput(Input *Input, bool IsPostLto);

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
  bool scanRelocations(bool IsPartialLink = false);

  void scanRelocationsHelper(InputFile *Input, bool IsPartialLink,
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
  bool relocation(bool EmitRelocs);

  /// finalizeSymbolValue - finalize the symbol value for symbols
  /// originating from object files.
  bool finalizeSymbolValues();

  /// finalizeSymbolValue - finalize the symbol value
  void finalizeSymbolValue(ResolveInfo *I) const;

  /// emitOutput - emit the output file.
  bool emitOutput(llvm::FileOutputBuffer &POutput);

  /// postProcessing - do modificatiion after all processes
  eld::Expected<void> postProcessing(llvm::FileOutputBuffer &POutput);

  // -----  readers and writers  ----- //
  ELFRelocObjParser *getRelocObjParser() const { return RelocObjParser; }

  ELFExecObjParser *getELFExecObjParser() const { return ELFExecObjParser; }

  BinaryFileParser *getBinaryFileParser() const { return BinaryFileParser; }

  ELFDynObjParser *getNewDynObjReader() const { return DynObjReader; }

  ArchiveParser *getArchiveParser() const { return ArchiveParser; }

  GroupReader *getGroupReader() const { return GroupReader; }

  ScriptReader *getScriptReader() const { return ScriptReader; }

  BitcodeReader *getBitcodeReader() const { return MPBitcodeReader; }

  ObjectReader *getSymDefReader() const { return SymDefReader; }

  ELFObjectWriter *getWriter() const { return ObjWriter; }

  /// LTO specific methods
  void setLTOPlugin(plugin::LinkerPlugin &LTOPlugin);
  bool createLTOObject();
  bool runAssembler(std::vector<std::string> &Files, std::string RelocModel,
                    const std::vector<std::string> &AsmFilesFromLto);
  void beginPostLTO();

  const GNULDBackend &getTargetBackend() const { return ThisBackend; }
  GNULDBackend &getTargetBackend() { return ThisBackend; }

  void assignOutputSections(std::vector<eld::InputFile *> &Inputs);

  // Sync Relocations.
  void syncRelocations(uint8_t *Buffer);

  void syncRelocationResult(uint8_t *Data, InputFile *Input);

  bool mergeInputSections(ObjectBuilder &Builder,
                          std::vector<Section *> &Sections);

  bool mayBeSortSections(std::vector<Section *> &Sections);

  bool createOutputSection(ObjectBuilder &Builder, OutputSectionEntry *Output,
                           bool PostLayout = false);

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

  bool getInputs(std::vector<InputFile *> &Inputs);

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

  void addInputSection(Section *InputSection) {
    AllInputSections.push_back(InputSection);
  }

  // -------------------------Backend input processing -------------
  bool processInputFiles();

  // -------------------------Backend symbols support --------------
  bool provideGlobalSymbolAndContents(std::string Name, size_t Sz,
                                      uint32_t Alignment);

  // --------------------Export LTO Phase --------------------------
  bool isPostLTOPhase() const { return MPostLtoPhase; }

  struct SymbolStats {
    uint64_t Global = 0;
    uint64_t Local = 0;
    uint64_t Weak = 0;
    uint64_t Hidden = 0;
    uint64_t Absolute = 0;
    uint64_t ProtectedSyms = 0;
    uint64_t File = 0;
  };

  const SymbolStats &getTotalSymbolStats() const;

  const SymbolStats &getDiscardedSymbolStats() const;

  /// Stores all the entry sections in SectionMap.
  void collectEntrySections() const;

  const ArchiveFile *
  getArchiveFileFromMemoryAreaToAFMap(const MemoryArea *MemArea) const {
    auto Iter = MMemoryAreaToArchiveFileMap.find(MemArea);
    if (Iter == MMemoryAreaToArchiveFileMap.end())
      return nullptr;
    return Iter->second;
  }

  void addToMemoryAreaToAFMap(const ArchiveFile &AF) {
    const MemoryArea *MemArea = AF.getInput()->getMemArea();
    MMemoryAreaToArchiveFileMap[MemArea] = &AF;
  }

private:
  std::unique_ptr<llvm::lto::LTO> ltoInit(llvm::lto::Config Conf);

  bool
  finalizeLtoSymbolResolution(llvm::lto::LTO &LTO,
                              const std::vector<BitcodeFile *> &BitCodeFiles);

  bool doLto(llvm::lto::LTO &LTO);

  /// writeRelocationResult - helper function of syncRelocationResult, write
  /// relocation target data to output
  void writeRelocationResult(Relocation &PReloc, uint8_t *POutput);

  /// writeRelocationData - another helper to write relocation data into the
  /// output
  void writeRelocationData(Relocation &PReloc, uint64_t Data, uint8_t *POutput);

  /// addSymbolToOutput - add a symbol to output symbol table if it's not a
  /// section symbol and not defined in the discarded section
  bool addSymbolToOutput(const ResolveInfo &PInfo) const;

  bool insertPostLTOELF();

  size_t getRelocSectSize(uint32_t Type, uint32_t Count);

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

  bool doVerifyPlugin(plugin::Plugin *P);

  bool mayBeAddSectionSymbol(ELFSection *S);

  std::string getLTOTempPrefix() const;

  llvm::Expected<std::unique_ptr<llvm::raw_fd_ostream>>
  createLTOTempFile(size_t Task, const std::string &Suffix,
                    llvm::SmallString<256> &FileName) const;

  /// Adds input files to outputTar if --reproduce option is used
  void addInputFileToTar(InputFile *Ipt, MappingFile::Kind K);

  void addInputFileToTar(const std::string &Name, MappingFile::Kind Kind);

  void addLTOOutputToTar();

  void addTempFilesToTar(size_t MaxTasks);

  // ---------------------extern-list, other extern list files parsing----------
  bool parseListFile(std::string File, uint32_t Type);

  void createCopyRelocation(ResolveInfo &Sym, Relocation::Type Type);

  void accountSymForSymStats(SymbolStats &SymbolStats, const ResolveInfo &RI);

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
  LinkerConfig &ThisConfig;
  Module *ThisModule;
  eld::IRBuilder *CurBuilder;

  GNULDBackend &ThisBackend;

  // -----  readers and writers  ----- //
  ELFRelocObjParser *RelocObjParser = nullptr;
  ELFDynObjParser *DynObjReader = nullptr;
  ArchiveParser *ArchiveParser = nullptr;
  ELFExecObjParser *ELFExecObjParser = nullptr;
  BinaryFileParser *BinaryFileParser = nullptr;
  GroupReader *GroupReader;
  ScriptReader *ScriptReader;
  ELFObjectWriter *ObjWriter;
  BitcodeReader *MPBitcodeReader;
  ObjectReader *SymDefReader;
  llvm::StringSet<> MDynlistExports;

  // Is this the second phase of normalize for LTO
  bool MPostLtoPhase;

  // Set to true once any GC pass has run. Used to know if shouldIgnore() on
  // a symbol is meaningful.
  bool MGcHasRun = false;

  bool MSaveTemps;

  bool MTraceLTO;

  std::string MLtoTempPrefix;

  // Paths of all generated LTO objects
  std::vector<std::string> LtoObjects;

  // Paths of other temporary files, that need to be cleaned up.
  std::vector<std::string> FilesToRemove;

  std::vector<Input *> LTOELFFiles;

  std::vector<WildcardPattern *> LTOPatternList;

  plugin::LinkerPlugin *LTOPlugin = nullptr;

  std::vector<Section *> AllInputSections;

  std::mutex Mutex;

  SymbolStats MTotalSymStats;

  SymbolStats MDiscardedSymStats;

  /// It is used to reuse ArchiveFileInfo when an archive is read multiple
  /// times. ArchiveFileInfo is obtained as 'archiveFile->getArchiveFileInfo()'.
  std::unordered_map<const MemoryArea *, const ArchiveFile *>
      MMemoryAreaToArchiveFileMap;
};

} // end namespace eld
#endif
