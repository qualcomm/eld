//===- LinkerWrapper.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_LINKERWRAPPER_H
#define ELD_PLUGINAPI_LINKERWRAPPER_H

#include "DiagnosticBuilder.h"
#include "DiagnosticEntry.h"
#include "Diagnostics.h"
#include "PluginADT.h"
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#define ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(LW, eldExp)                     \
  if (!eldExp) {                                                               \
    (LW)->reportDiagEntry(std::move(eldExp.error()));                          \
    return;                                                                    \
  }

#define ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(LW, eldExp)                    \
  if (!eldExp) {                                                               \
    (LW)->reportDiagEntry(std::move(eldExp.error()));                          \
    return ::eld::plugin::Plugin::Status::ERROR;                               \
  }

#define ELDEXP_REPORT_AND_RETURN_FALSE_IF_ERROR(LW, eldExp)                    \
  if (!eldExp) {                                                               \
    (LW)->reportDiagEntry(std::move(eldExp.error()));                          \
    return false;                                                              \
  }

#define ELDEXP_RETURN_DIAGENTRY_IF_ERROR(eldExp)                               \
  if (!eldExp)                                                                 \
    return std::move(eldExp.error());

namespace llvm {
class Timer;
}

namespace eld {
class DiagnosticEngine;
class INIWriter;
class Module;
class Section;
class Plugin;

namespace plugin {
struct DLL_A_EXPORT Chunk;
struct DLL_A_EXPORT DynamicLibrary;
struct DLL_A_EXPORT DWARFInfo;
struct DLL_A_EXPORT INIFile;
struct DLL_A_EXPORT LinkerScript;
struct DLL_A_EXPORT LinkerScriptRule;
class DLL_A_EXPORT MemoryBuffer;
struct DLL_A_EXPORT OutputSection;
struct DLL_A_EXPORT PluginData;
struct DLL_A_EXPORT RelocationHandler;
struct DLL_A_EXPORT Section;
struct DLL_A_EXPORT Segment;
struct DLL_A_EXPORT SmallJSONValue;
struct DLL_A_EXPORT Symbol;
struct DLL_A_EXPORT Use;
class DLL_A_EXPORT TarWriter;

using AuxiliarySymbolNameMap = std::unordered_map<uint64_t, std::string>;

/// LinkerWrapper provides a bridge between the linker plugin and the linker.
/// The plugins uses a LinkerWrapper object to interact and communicate with
/// the linker.
///
/// \note Even though, all LinkerWrapper APIs are available to all plugin
/// interface types at all link stages, some APIs are only meaningful for
/// certain plugin interface types and link stages.
class DLL_A_EXPORT LinkerWrapper {
public:
  /// Stages of the link process.
  /// What actions a plugin can perform depends on the
  /// link state. For example:
  /// - Plugins can change the output section of an input
  ///   section only in BeforeLayout link state.
  /// - Plugins can move chunks from one output section to
  ///   another only in CreatingSections link state.
  /// - Plugins can only compute the output image layout checkum using
  ///   `LinkerWrapper::getImageLayoutChecksum` only in AfterLayout linker
  ///   state.
  /// - Plugins can reassign virtual addresses using
  ///   `LinkerWrapper::reassignVirtualAddresses()` only in CreatingSegments
  ///   link state.
  ///
  /// Plugin authors should ensure that the action being performed by the
  /// plugin is meaningful in the link state in which it is executed.
  /// Executing invalid actions for a link state can result in undefined
  /// behavior.
  enum State {
    Unknown,
    Initializing,
    BeforeLayout,
    CreatingSections,
    AfterLayout,
    // CreatingSegments runs BEFORE AfterLayout despite having a greater value.
    CreatingSegments,
  };

  /// Link modes.
  enum LinkMode : uint8_t {
    UnknownLinkMode,
    StaticExecutable,
    DynamicExecutable,
    SharedLibrary,
    PIE,
    PartialLink
  };

  /// Represents an unbalanced chunk move.
  /// For unbalanced chunk remove:
  /// chunk: The removed chunk
  /// rule: Rule from which it was removed.
  ///
  /// For unbalanced chunk add:
  /// chunk: The added chunk
  /// rule: Rule to which it was added.
  struct DLL_A_EXPORT UnbalancedChunkMove {
    Chunk chunk;
    LinkerScriptRule rule;
  };

  using CommandLineOptionHandlerType = std::function<void(
      const std::string &, const std::optional<std::string> &)>;

  /// Construct LinkerWrapper object.
  explicit LinkerWrapper(eld::Plugin *, eld::Module &);

  /// Tell the linker that garbage collection should run. This method must be
  /// called early, at least no later than when symbols are created, for example
  /// from Init(). This call will set the initial state of each symbol before
  /// garbage collection and force the GC pass.
  /// This is not really needed, but avoiding this will require a bit of
  /// refactoring.
  void RequestGarbageCollection();

  /// Returns a vector of Uses of symbols defined in chunk C.
  /// \param C Chunk containing the symbol definition.
  /// \return vector of Uses that are referred from the Chunk C.
  ///
  /// \note This function's result is valid for the link states
  /// \em BeforeLayout and beyond.
  eld::Expected<std::vector<Use>> getUses(Chunk &C);

  /// Returns eld version
  std::string getLinkerVersion() const;

  /// Returns a vector of Uses that are referred from the Section S.
  ///
  /// \param S Section
  /// \return vector of uses that are referred from the Section S.
  ///
  /// \note This function's result is valid for the link states
  /// \em BeforeLayout and beyond.
  eld::Expected<std::vector<Use>> getUses(Section &S);

  /// Returns the linker symbol Sym.
  ///
  /// \param Sym Symbol name
  /// \returns if successfully found, then Symbol object which
  /// represents the specified symbol; Otherwise an empty Symbol
  /// object.
  ///
  /// \note This function's result is valid for the link states
  /// \em BeforeLayout and beyond.
  eld::Expected<Symbol> getSymbol(const std::string &Sym) const;

  /// Create a symbol but do not provide symbol resolution information (chunk or
  /// value). Used to define bitcode symbols. Note that InputSection is
  /// optional.
  /// IMPORTANT: The symbol is resolved right away and the returned symbol
  /// will reference the resolved symbol. This means that the returned symbol
  /// may not be the bitcode symbol.
  eld::Expected<Symbol>
  addSymbol(InputFile InputFile, const std::string &Name,
            plugin::Symbol::Binding Binding, Section InputSection,
            plugin::Symbol::Kind Kind,
            plugin::Symbol::Visibility Visibility = plugin::Symbol::Default,
            unsigned Type = 0, uint64_t Size = 0);

  /// Override the output section to OutputSection for Section S.
  /// \param S Section for which the output section needs to be overriden.
  /// \param OutputSection Name of the output section.
  /// \param Annotation Annotations are used for diagnostic purposes.
  ///
  /// \note This function must only be used in \em BeforeLayout link state.
  eld::Expected<void> setOutputSection(Section &S,
                                       const std::string &OutputSection,
                                       const std::string &Annotation = "");

  /// Mark the symbol as preserved for garbage collection.
  eld::Expected<void> setPreserveSymbol(Symbol Symbol);

  eld::Expected<plugin::Section> createBitcodeSection(const std::string &Name,
                                                      BitcodeFile BitcodeFile,
                                                      bool IsInternal = false);

  /// Rename a section.
  void setSectionName(plugin::Section, const std::string &);

  /// For a given section, set its input used for rule matching. Matching
  /// input file must be set for post-LTO sections.
  void setRuleMatchingInput(plugin::Section, plugin::InputFile);

  /// Add a reference from ReferencingSection to ReferencedSymbol.
  eld::Expected<void> addReferencedSymbol(plugin::Section ReferencingSection,
                                          plugin::Symbol ReferencedSymbol);

  // Run a garbage collection pass. If a plugin calls this method, it must have
  // called RequestGarbageCollection before reading input files.
  void runGarbageCollection(const std::string &Phase);

  /// Return the corresponding output section for the section S.
  ///
  /// \param S Input section
  /// \returns corresponding output section for the section S.
  ///
  /// \note This function's result is valid for link states \em BeforeLayout
  /// and beyond.
  /// \note This function's result is invalid for \em SectionIteratorPlugin run
  /// even though the link state is \em Beforelayout.
  eld::Expected<OutputSection> getOutputSection(Section &S) const;

  /// Finds an output section with the name OutputSection.
  ///
  /// \param OutputSection Output section name
  /// \return If successfully found, an output section representing the
  /// specified output section; Otherwise an empty plugin::OutputSection
  /// object.
  ///
  /// \note This function's result is valid for link states \em BeforeLayout
  /// and beyond.
  /// \note This function's result is invalid for \em SectionIteratorPlugin run
  /// even though the link state is \em Beforelayout.
  eld::Expected<OutputSection>
  getOutputSection(const std::string &OutputSection);

  /// \return contents of this output section
  eld::Expected<std::unique_ptr<const uint8_t[]>>
  getOutputSectionContents(OutputSection &O) const;

  /// Finish assigning output sections.
  ///
  /// This function should be called by the plugin after all the section
  /// rule-matching modifications has been done by the plugin.
  ///
  /// \note If the plugin has not changed any section rule-matching, then
  /// there is no need to call this function.
  /// \note This function must only be used in \em BeforeLayout link state.
  eld::Expected<void> finishAssignOutputSections();

  /// This function may be called to reassign section addresses to reflect
  /// newly added output sections that have not yet been assigned an address.
  /// \note This function may only be used in \em CreatingSegments state.
  eld::Expected<void> reassignVirtualAddresses();

  /// Set the linker to fatally error. This function will cause link
  /// process to terminate as soon as it can. It must be used if a plugin has
  /// encountered an unrecoverable error.
  void setLinkerFatalError();

  /// Returns the underlying linker plugin associated with the linker wrapper.
  eld::Plugin *getPlugin() const { return m_Plugin; }

  /// Returns true if the string Name matches the pattern specified by
  /// Pattern; Otherwise returns false.
  ///
  /// Pattern matching here follows unix glob style pattern matching.
  ///
  /// \param Pattern glob pattern
  /// \param name string to be matched against the pattern
  /// \returns true if Name matches the pattern specified by Pattern;
  /// Otherwise returns false.
  bool matchPattern(const std::string &Pattern, const std::string &Name) const;

  /// Destroy the Linker Wrapper.
  // TODO: Default destructor works well. Why do we need explicit destructor?
  ~LinkerWrapper();

  /// Returns the current link state.
  State getState() const;

  /// Return true if the linker is processing post LTO objects.
  bool isPostLTOPhase() const;

  /// Adds a local symbol Symbol to the chunk C.
  ///
  /// \param C Chunk in which the symbol should be added.
  /// \param symbol Name of the symbol
  /// \param Val Value of the symbol
  eld::Expected<void> addSymbolToChunk(Chunk &C, const std::string &Symbol,
                                       uint64_t Val);

  /// Creates a Padding Chunk in the section,
  /// '.bss.paddingchunk.${PluginName}', having contents as 0x0.
  ///
  /// \param Alignment Required alignment for the padding chunk
  /// \param PaddingSize Required size for the padding chunk
  /// \returns The created padding chunk
  eld::Expected<Chunk> createPaddingChunk(uint32_t Alignment,
                                          size_t PaddingSize);

  /// Creates a code chunk in the section,
  /// '.text.codechunk.${Name}.${PluginName}', having contents specified
  /// by the buffer Buf of size Sz. The created Code chunk will have all the
  /// usual properties of the code section such as PROGBITS section type, and
  /// allocation and execution permissions.
  ///
  /// \param Name It is used to determined the section name of the chunk.
  /// \param Alignment Alignment to be used for the chunk
  /// \param Buf Buffer containing the data that the the chunk will store.
  /// \param Sz Size of the buffer Buf
  /// \returns The created code chunk
  eld::Expected<Chunk> createCodeChunk(const std::string &Name,
                                       uint32_t Alignment, const char *Buf,
                                       size_t Sz);

  /// Creates a data chunk in the section,
  /// '.data.codechunk.${Name}.${PluginName}', having contents specified
  /// by the buffer Buf of size Sz. Data chunk will have all the usual
  /// properties of the data section such as PROGBITS section type, and
  /// allocation and write permissions.
  ///
  /// \param Name It is used to determine the section name of the chunk
  /// \param Alignment Alignment to be used for the chunk
  /// \param Buf Buffer containing the data that the chunk will store.
  /// \param Sz Size of the buffer Buf
  /// \returns The created data chunk.
  eld::Expected<Chunk> createDataChunk(const std::string &Name,
                                       uint32_t Alignment, const char *Buf,
                                       size_t Sz);

  /// Creates a data chunk in the section,
  /// 'Name', having contents specified
  /// by the buffer Buf of size Sz. Data chunk will have all the usual
  /// properties of the data section such as PROGBITS section type, and
  /// allocation and write permissions.
  ///
  /// \param Name It is used to determine the section name of the chunk
  /// \param Alignment Alignment to be used for the chunk
  /// \param Buf Buffer containing the data that the chunk will store.
  /// \param Sz Size of the buffer Buf
  /// \returns The created data chunk.
  eld::Expected<Chunk> createDataChunkWithCustomName(const std::string &Name,
                                                     uint32_t Alignment,
                                                     const char *Buf,
                                                     size_t Sz);

  /// Create a Data Chunk in .bss section
  eld::Expected<Chunk> createBSSChunk(const std::string &Name,
                                      uint32_t Alignment, size_t Sz);

  // FIXME: Is it okay for alignment to be of 32-bit size? sh_addralign is of 8
  // bytes for 64-bit ELF.
  eld::Expected<Chunk> createChunkWithCustomName(const std::string &name,
                                                 size_t sectType,
                                                 size_t sectFlags,
                                                 uint32_t alignment,
                                                 const char *buf, size_t sz);

  /// Replace symbol S content with the buffer Buf of size Sz.
  ///
  /// \param S Symbol whose content has to be replaced.
  /// \param Buf Buffer containing the new content.
  /// \param Sz Buffer size
  /// \returns \em void if the symbol's content is successfully
  /// replaced; Otherwise returns a DiagnosticEntry object
  /// (as std::unique_ptr<DiagnosticEntry>) describing the error.
  ///
  /// \note This function can be used in any link state upto and including
  /// \em AfterLayout.
  eld::Expected<void> replaceSymbolContent(plugin::Symbol S, const uint8_t *Buf,
                                           size_t Sz);

  /// Adds the chunk C to the LinkerScriptRule rule.
  ///
  /// \param Rule Linker script rule to which the chunk C must be added.
  /// \param C Chunk that is to be added.
  /// \param A Annotation to be used for diagnostics purpose.
  ///
  /// \note This function must only be used in \em CreatingSections
  /// link state.
  eld::Expected<void> addChunk(const LinkerScriptRule &Rule, const Chunk &C,
                               const std::string &A = "");

  /// Removes the chunk C from the output section of the rule.
  ///
  /// \param Rule Linker script rule from which chunk has to be removed
  /// \param C Chunk that is to be removed.
  /// \param A Annotation to be used for diagnostics purpose.
  ///
  /// \note This function must only be used in \em CreatingSections
  /// link state.
  eld::Expected<void> removeChunk(const LinkerScriptRule &Rule, const Chunk &C,
                                  const std::string &A = "");

  /// Reset the chunks of the output section of the rule to the sequence of
  /// chunks specified by V. More precisely, this function removes all the
  /// existing chunks from the output sections, and adds the sequence of chunks
  /// , in order, as specified by V.
  ///
  /// \param Rule Linker script rule whose output section's chunks have to be
  /// updated.
  /// \param V Sequence of chunks that are to be added to the output
  /// section of the rule.
  /// \param A Annotation to be used for diagnostics
  /// purposes.
  ///
  /// \note This function must only be used in \em CreatingSections
  /// link state.
  eld::Expected<void> updateChunks(const LinkerScriptRule &Rule,
                                   const std::vector<Chunk> &V,
                                   const std::string &A = "");

  /// Returns a string containing Linker and LLVM repository version
  /// informations.
  std::string getRepositoryVersion() const;

  /// Record Plugin Data from a plugin.
  void recordPluginData(uint32_t Key, void *Data,
                        const std::string &Annotation);

  /// Return the recorded plugin data.
  std::vector<PluginData> getPluginData(const std::string &PluginName);

  /// Profiling Plugin support.
  llvm::Timer *CreateTimer(const std::string &Name,
                           const std::string &Description, bool IsEnabled);

  /// Register a relocation for the plugin to call back.
  eld::Expected<void> registerReloc(uint32_t RelocType,
                                    const std::string &Name = "");

  /// Returns the relocation handler.
  RelocationHandler getRelocationHandler() const;

  /// Returns true if the linker is using multiple threads.
  bool isMultiThreaded() const;

  /// Returns number of threads used by the linker.
  size_t getPluginThreadCount() const;

  /// Return the associated LinkerScript
  LinkerScript getLinkerScript();

  /// Get FileName file contents
  ///
  /// If FileName doesn't exist, then returns an empty string.
  std::string getFileContents(std::string FileName);

  /// \return path to file if it is found. If not found, then an
  /// Error DiagnosticEntry is returned.
  eld::Expected<std::string> findConfigFile(const std::string &FileName) const;

  /// Read a config file in ini format.
  /// \param FileName ini file to read
  /// \returns eld::Expected<INIFile> object containing either the
  /// successfully read INI file, or a diagnostic object describing the
  /// failure..
  eld::Expected<INIFile> readINIFile(std::string FileName);

  /// Write an ini file.
  /// \param ini INIFile
  /// \param OutputPath path of the file to write
  /// \returns std::unique_ptr<DiagnosticEntry> object that describes the error;
  /// if no error occurs, then returns void.
  eld::Expected<void> writeINIFile(INIFile &INI,
                                   const std::string &OutputPath) const;

  /// Creates and returns eld::Expected<TarWriter> to support
  /// creating a tar archive with the name passed and add files to it.
  /// \param std::string &Name for tar file name.
  /// \returns eld::Expected<TarWriter>
  eld::Expected<TarWriter> getTarWriter(const std::string &Name) const;

  /// \returns true if Timing is enabled for this plugin;
  /// Otherwise returns false.
  bool isTimingEnabled() const;

  /// \returns the input object files visited by the Linker.
  /// \note This function do not returns the input linker scripts.
  std::vector<plugin::InputFile> getInputFiles() const;

  /// Returns the link mode.
  LinkMode getLinkMode() const;

  // --- DWARF Support

  eld::Expected<DWARFInfo> getDWARFInfoForInputFile(plugin::InputFile F,
                                                    bool is32bit) const;

  // Delete DWARFContext for an Input File
  eld::Expected<void> deleteDWARFInfoForInputFile(plugin::InputFile &F) const;

  // --- JSON Support
  bool writeSmallJSONFile(const std::string &FileName, SmallJSONValue &V) const;

  /// Return true if 32 bit target
  bool is32Bits() const;

  bool is64Bits() const;

  /// Allocate an uninitialized buffer that will live throughout the link
  /// process.
  ///
  /// \param S Size of the uninitialized buffer
  /// \returns base address of the created uninitialized buffer.
  char *getUninitBuffer(size_t S) const;

  /// Reset an undefined symbol to another chunk. Reset here means, change the
  /// source chunk of the symbol. This functions marks that symbol S is defined
  /// in the chunk C.
  ///
  /// \param S Symbol whose source chunk is to be changed.
  /// \param C New source chunk for the symbol.
  /// \returns true if resetting is successful; Otherwise false.
  ///
  /// \note This function can be used in any link state including
  /// and upto \em Afterlayout.
  /// \note This function only works for symbols that do not have a value.
  /// The function returns false, if the symbol requested to
  /// be reset is a defined symbol with an initial value.
  eld::Expected<void> resetSymbol(plugin::Symbol S, Chunk C);

  /// Create and return a Use for a Chunk, and add it to the Chunk.
  eld::Expected<Use> createAndAddUse(Chunk C, off_t Offset,
                                     uint32_t RelocationType, plugin::Symbol S,
                                     int64_t Addend);

  /// Return the value of the relocation target
  eld::Expected<void> getTargetDataForUse(Use &U, uint64_t &Data) const;

  /// Set the value of the relocation target
  eld::Expected<void> setTargetDataForUse(Use &U, uint64_t Data);

  /// Returns the 32-bit checksum of the image layout if this function is
  /// used when the linker state is \em AfterLayout; Otherwise returns 0.
  ///
  /// \note This function's result is only valid when the link
  /// state is \em AfterLayout.
  eld::Expected<uint32_t> getImageLayoutChecksum() const;

  /// Returns the output file name of the current link process.
  std::string getOutputFileName() const;

  /// Create a Linker script rule.
  ///
  /// \param S Corresponding output section of the linker script rule.
  /// \param Annotation It is used to create the rule name. It's also helpful
  /// for debugging purposes.
  ///
  /// \note Annotation must not be empty. If annotation is empty,
  /// then this function returns an empty LinkerScriptRule object.
  eld::Expected<LinkerScriptRule>
  createLinkerScriptRule(OutputSection S, const std::string &Annotation);

  /// Add a linker script rule after an existing rule.
  /// A rule describes an input section description.
  ///
  /// \note This function must only be used when the link state is
  /// \em CreatingSections. Behavior of using this function in other linker
  /// states is undefined.
  ///
  /// \param O Output section of the rule.
  /// \param Rule Rule after which RuleToAdd have to be inserted.
  /// \param RuleToAdd Rule that is to be added.
  /// \returns true if rule is successfully inserted; Otherwise false.
  eld::Expected<void> insertAfterRule(plugin::OutputSection O,
                                      LinkerScriptRule Rule,
                                      LinkerScriptRule RuleToAdd);

  /// Add a linker script rule before an existing rule.
  ///
  /// \note This function should only be used when the linker state is
  /// \em CreatingSections. Behavior of using this function in other linker
  /// states is undefined.
  ///
  /// \param O Output section of the rule.
  /// \param Rule Rule before which RuleToAdd have to be inserted.
  /// \param RuleToAdd Rule that is to be added.
  /// \returns true if rule is successfully inserted; Otherwise false.
  eld::Expected<void> insertBeforeRule(plugin::OutputSection O,
                                       LinkerScriptRule Rule,
                                       LinkerScriptRule RuleToAdd);

  /// Exclude the symbol S from the symbol table.
  ///
  /// \param S Symbol to remove
  ///
  /// This function only removes the symbol from the symbol table. It does
  /// not remove the symbol content from the output image.
  ///
  /// \note This function can be used in any link state including and upto
  /// \em AfterLayout.
  DEPRECATED void removeSymbolTableEntry(plugin::Symbol S);

  /// Returns all symbols.
  ///
  /// \note Currently, the function returns an empty vector for
  /// SectionIteratorPlugin and SectionMatcherPlugin.
  /// \note The function returns all non-section symbol names for
  /// OutputSectionIteratorPlugin when linker state is including and upto
  /// \em CreatingSections. The function returns all symbols names,
  /// including section symbol names, when the linker state is
  /// \em AfterLayout.
  eld::Expected<std::vector<plugin::Symbol>> getAllSymbols() const;

  /// Returns an ID for a fatal error diagnostic with the specified format
  /// string.
  DiagnosticEntry::DiagIDType getFatalDiagID(const std::string &formatStr);
  /// Returns an ID for an error diagnostic with the specified format
  /// string.
  DiagnosticEntry::DiagIDType getErrorDiagID(const std::string &formatStr);
  /// Returns an ID for a warning diagnostic with the specified format
  /// string.
  DiagnosticEntry::DiagIDType getWarningDiagID(const std::string &formatStr);

  /// Returns an ID for a note diagnostic with the specified format
  /// string.
  DiagnosticEntry::DiagIDType getNoteDiagID(const std::string &formatStr) const;

  /// Returns an ID for a verbose diagnostic with the specified format
  /// string.
  DiagnosticEntry::DiagIDType getVerboseDiagID(const std::string &formatStr);

  /// Issue a diagnostic using DiagnosticEntry.
  ///
  /// \returns true if diagnostic is successfully emitted; Otherwise returns
  /// false.
  bool reportDiagEntry(std::unique_ptr<DiagnosticEntry> de);

  /// Report a diagnostic.
  template <class... Args>
  bool reportDiag(DiagnosticEntry::DiagIDType id, Args &&...args) const {
    DiagnosticBuilder diagBuilder = getDiagnosticBuilder(id);
    return reportDiag(diagBuilder, std::forward<Args>(args)...);
  }

  /// Create and return a DiagnosticBuilder for the diagnostic ID 'id'.
  ///
  /// For all cases, reporting diagnostics using 'LinkerWrapper::report' should
  /// be preferred. DiagnosticBuilder can be beneficial in some cases. One such
  /// case is to reduce code-duplication when emitting same family of
  /// diagnostics.
  ///
  /// DiagnosticBuilder should be used carefully as incorrect usage
  /// can cause deadlock in a program. In particular, no two DiagnosticBuilder
  /// objects should be in scope at the same time in the same thread.
  DiagnosticBuilder getDiagnosticBuilder(DiagnosticEntry::DiagIDType id) const;

  /// Allow plugins to determine if a chunk can be moved out of an output
  /// section and placed in a different output section.
  ///
  /// \param C Chunk
  ///
  /// \returns true if Chunk can be moved
  ///
  /// \note This function needs to be used by plugins before moving any Chunk
  /// as a safety net
  bool isChunkMovableFromOutputSection(const Chunk &C) const;

  /// Returns the string representation of the current link state.
  std::string_view getCurrentLinkStateAsStr() const;

  /// Returns true if user has requested verbose diagnostics.
  bool isVerbose() const;

  eld::Expected<std::vector<plugin::OutputSection>>
  getAllOutputSections() const;

  eld::Expected<std::vector<Segment>>
  getSegmentsForOutputSection(const OutputSection &O) const;

  /// \return the segment table
  eld::Expected<std::vector<Segment>> getSegmentTable() const;

  /// Applies relocations to an unrelocated buffer `Buf`.
  eld::Expected<void> applyRelocations(uint8_t *Buf);

  /// Apply all relocations or return an error.
  eld::Expected<void> doRelocation();

  /// Return a handle to the library `LibraryName` or an error.
  /// `LibraryName` will be searched using the linker search path
  /// including -L directories. First the file called `LibraryName` will be
  /// searched. If it is not found, `libLibraryName.so` or `libraryName.dll` on
  /// windows will be searched. If neither are found an error is returned.
  eld::Expected<DynamicLibrary> loadLibrary(const std::string &LibraryName);

  /// \return a handle to the function `FunctionName` or an error.
  eld::Expected<void *> getFunction(void *LibraryHandle,
                                    const std::string &FunctionName) const;
  /// Returns a vector of (Chunk, LinkerScriptRule) pairs for chunks that were
  /// removed by the plugin but have not been inserted back to the output image.
  std::vector<UnbalancedChunkMove> getUnbalancedChunkRemoves() const;

  /// Returns a vector of (Chunk, LinkerScriptRule) pairs for chunks that are
  /// inserted to a rule but are not removed from their original rule. This
  /// typically happens when a plugin inserts a chunk from rule A to rule
  /// B but forgets to remove it from rule A.
  std::vector<UnbalancedChunkMove> getUnbalancedChunkAdds() const;

  /// Add chunk C to the output. The output section where C lives will be
  /// deterimned according to the linker script.
  eld::Expected<void> addChunkToOutput(Chunk C);

  /// Clear the offset for this section to be reassigned later by the linker.
  eld::Expected<void> resetOffset(OutputSection O);

  /// Return the OutputSection and LinkerScriptRule for a given Input section
  /// according to the linker script.
  eld::Expected<std::pair<OutputSection, LinkerScriptRule>>
  getOutputSectionAndRule(Section S);

  /// Returns the value of the environment variable envVar.
  std::optional<std::string> getEnv(const std::string &envVar) const;

  /// Link Section A to B using the sh_link field in the output ELF.
  eld::Expected<void> linkSections(OutputSection A, OutputSection B) const;

  /// Registers the command-line option `opt`.
  ///
  /// `optionHandler` callback function is invoked for each registered
  /// option present in the link command-line.
  ///
  /// `opt` must always start with `--`. If `hasValue` is true, then options
  /// of the form `--<OptionName>=<value>` are matched; Otherwise options of
  /// the form `--<OptionName>` are matched.
  ///
  /// \note `opt` must not match any in-built linker command-line option.
  eld::Expected<void>
  registerCommandLineOption(const std::string &opt, bool hasValue,
                            const CommandLineOptionHandlerType &optionHandler);

  /// Enables VisitSymbol hook. VisitSymbol hook is disabled by default.
  eld::Expected<void> enableVisitSymbol();

  /// Sets rule-matching section name map for the input file IF.
  /// Rule matching section name map contains the mapping of section index to
  /// the rule-matching section name.
  ///
  /// If a section has rule-matching section name, then this name is used,
  /// instead of the actual section name, for linker script rule-matching.
  /// For example, if for an input file IF, the rule-matching
  /// section name map is as shown below, and section with index 3 is
  /// '.text.foo', then for linker script rule-matching, then linker will use
  /// the name '.code.foo' instead of '.text.foo'.
  ///
  /// {
  ///   3 -> .code.foo
  ///   ...
  ///   ...
  /// }
  ///
  /// \note Rule-matching section map can be set only once per input-file.
  /// Setting a rule-matching section name map for an input file is an error
  /// if the file already has one.
  eld::Expected<void> setRuleMatchingSectionNameMap(
      InputFile IF, std::unordered_map<uint64_t, std::string> sectionMap);

  /// Returns the rule-matching section name map for the input file IF.
  const std::optional<std::unordered_map<uint64_t, std::string>> &
  getRuleMatchingSectionNameMap(const InputFile &IF);

  std::optional<std::string> getRuleMatchingSectionName(Section S);

  plugin::LinkerConfig getLinkerConfig() const;

  /// Instruct linker to show rule-matching section name in diagnostics and
  /// map-files.
  void showRuleMatchingSectionNameInDiagnostics();

  /// Sets the auxiliary symbol name map for the input file IF.
  /// Auxiliary symbol name map contains the mapping of symbol index to
  /// the auxiliary symbol name.
  ///
  /// Auxiliary symbol names are printed along with the original symbol names in
  /// the diagnostics and map-files. The auxiliary symbol names are helpful for
  /// providing more meaningful diagnostics.
  ///
  /// Auxiliary symbol names do not affect the link behavior.
  ///
  /// \note Auxiliary symbol name map can be set only once per input-file.
  /// Setting an auxiliary symbol name map for an input file is an error
  /// if the file already has one.
  eld::Expected<void>
  setAuxiliarySymbolNameMap(InputFile IF,
                            const AuxiliarySymbolNameMap &symbolNameMap);

  /// Returns the index of the symbol S in the output image symbol table.
  eld::Expected<uint64_t> getOutputSymbolIndex(plugin::Symbol S);

  /// Returns true if the rule R matches the section S.
  ///
  /// If doNotUseRMName is true, then the section actual name is used for
  /// rule-matching instead of the rule-matching name. By default, rule-matching
  /// name is preferred over the section name.
  ///
  /// This function is thread-safe.
  eld::Expected<bool> doesRuleMatchWithSection(const LinkerScriptRule &R,
                                               const Section &S,
                                               bool doNotUseRMName = false);

private:
  /// Report a diagnostic using diagnostic builder and the
  /// diagnostic arguments.
  template <class Arg, class... Args>
  bool reportDiag(DiagnosticBuilder &diagBuilder, Arg &&arg,
                  Args &&...args) const {
    diagBuilder << arg;
    return reportDiag(diagBuilder, std::forward<Args>(args)...);
  }

  /// Base case for the recursive 'report' function.
  bool reportDiag(DiagnosticBuilder &diagBuilder) const { return true; }

  /// Returns the linker module.
  eld::Module *getModule() const { return &m_Module; }

private:
  eld::Module &m_Module;
  eld::Plugin *m_Plugin;
  eld::DiagnosticEngine *m_DiagEngine;
  friend class LayoutWrapper;
};
} // namespace plugin
} // namespace eld

#endif
