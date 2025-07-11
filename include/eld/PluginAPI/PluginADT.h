//===- PluginADT.h---------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_PLUGINADT_H
#define ELD_PLUGINAPI_PLUGINADT_H

#include "DWARF.h"
#include "Defines.h"
#include "DiagnosticEntry.h"
#include "Expected.h"
#include "LinkerScript.h"
#include "ThreadPool.h"
#include <string>
#include <sys/types.h>
#include <vector>

namespace eld {
class BitcodeFile;
class BranchIsland;
class ELFSection;
class ELFSegment;
class Fragment;
class INIReader;
class InputFile;
class LDSymbol;
class LinkerConfig;
class LinkerScript;
class MemoryArea;
struct MergeableString;
class OutputSectionEntry;
class PluginData;
class Relocation;
class Relocator;
class ResolveInfo;
class RuleContainer;
class Section;
class SymbolInfo;
} // namespace eld

namespace llvm {
class Timer;

namespace lto {
class InputFile;
}
} // namespace llvm

namespace eld::plugin {

struct DLL_A_EXPORT BitcodeFile;
struct DLL_A_EXPORT Chunk;
struct DLL_A_EXPORT DynamicLibrary;
struct DLL_A_EXPORT InputFile;
struct DLL_A_EXPORT INIFile;
struct DLL_A_EXPORT LinkerScript;
struct DLL_A_EXPORT OutputSection;
class DLL_A_EXPORT Plugin;
struct DLL_A_EXPORT PluginData;
struct DLL_A_EXPORT Section;
struct DLL_A_EXPORT Segment;
struct DLL_A_EXPORT Symbol;
struct DLL_A_EXPORT Use;
class DLL_A_EXPORT MemoryBuffer;
class DLL_A_EXPORT LinkerWrapper;
struct DLL_A_EXPORT MergeableString;
class DLL_A_EXPORT TarWriter;
struct DLL_A_EXPORT LinkerConfig;

/// A section is formed of a set of chunks. Chunk is a handler for one such
/// section chunk. A Chunk object can be used to inspect the properties,
/// symbols and content of a chunk.
///
/// Chunk is a wrapper for an internal fragment class. If two Chunk objects
/// both refer to the same underlying fragment, then they represent the same
/// chunk.

struct DLL_A_EXPORT Chunk {

  explicit Chunk(eld::Fragment *F) : m_Fragment(F) {}

  Chunk() : m_Fragment(nullptr) {}

  /// Returns the name of the section that contains the chunk.
  /// If the object is an empty handler, then returns an empty string.
  std::string getName() const;

  /// Returns the input file handler of the file from which the chunk has
  /// originated. If the object is an empty handler, then returns an empty input
  /// file handler.
  plugin::InputFile getInputFile() const;

  /// Returns the size of the chunk.
  uint32_t getSize() const;

  /// Returns the alignment of the chunk.
  uint32_t getAlignment() const;

  /// Returns the section from which the chunk has originated.
  plugin::Section getSection() const;

  /// Returns the pointer to the underlying fragment object of the Chunk.
  eld::Fragment *getFragment() const { return m_Fragment; }

  /// Returns the raw data that the Chunk contains.
  const char *getRawData() const;

  /// Returns the list of symbols that are defined in this chunk.
  std::vector<plugin::Symbol> getSymbols() const;

  /// Returns true if the chunks A and B originates
  /// from different input sections; Otherwise returns false.
  struct DEPRECATED DLL_A_EXPORT Compare final {
    bool operator()(const Chunk &A, const Chunk &B) const;
  };

  //
  // Operators
  //

  /// Pointer comparison between the underlyling fragment pointers of LHS
  /// and RHS.
  bool operator<(const Chunk &RHS) const { return m_Fragment < RHS.m_Fragment; }

  /// Pointer comparison between the underlyling fragment pointers of LHS
  /// and RHS.
  bool operator>(const Chunk &RHS) const { return m_Fragment > RHS.m_Fragment; }

  /// Returns true if both the LHS and RHS represents the same chunk.
  bool operator==(const Chunk &RHS) const {
    return m_Fragment == RHS.m_Fragment;
  }

  /// Returns true if the underlying fragment pointer is non-null.
  operator bool() const { return m_Fragment != nullptr; }

  /// Returns true if the underlying fragment pointer is non-null.
  bool hasContent() const;

  /// Returns true if the chunk is stored in the image. In ELF, such chunks are
  /// from sections of the PROGBITS type.
  bool isProgBits() const;

  /// Returns true if the chunk is not stored in the image. In ELF, such chunks
  /// are from sections of the NOBITS type.
  bool isNoBits() const;

  /// Returns true if the chunk contains code. More concretely,
  /// return true if the section from which the chunk originates
  /// has SHF_EXECINSTR flag set.
  bool isCode() const;

  /// Returns true if the chunk is allocated in memory. In ELF, such chunks are
  /// from sections that have the ALLOC attribute.
  bool isAlloc() const;

  /// Returns true if the chunk is writable. In ELF, such chunks are from
  /// sections that have the WRITE attribute.
  bool isWritable() const;

  /// Does this chunk contain mergeable strings ?
  bool isMergeableString() const;

  /// Retrieve the address of the Chunk.
  size_t getAddress() const;

  /// Returns a sequence of all the sections that are dependent on the
  /// section from which the chunk has originated.
  std::vector<plugin::Section> getDependentSections() const;

protected:
  // Internal Data structure.
  eld::Fragment *m_Fragment;
};

/// A MergeStringChunk is a Chunk that represents the contents of a SHF_MERGE &
/// SHF_STRINGS section with alignment and entry size of 1. This Chunk contains
/// null-terminated strings that the linker may  merge with other identical
/// strings. For a Chunk to be a MergeStringChunk, isMergeableString() must be
/// true for that Chunk.
struct DLL_A_EXPORT MergeStringChunk : public Chunk {
  MergeStringChunk(eld::Fragment *F) : Chunk(F) {}

  /// Return the strings contained in this Chunk in a vector
  std::vector<plugin::MergeableString> getStrings() const;
};

/// MergeableString represents a null-terminated string inside a Chunk of a
/// section that has the flags SHF_STRINGS & SHF_MERGE, and alignment and entry
/// size of 1.
struct DLL_A_EXPORT MergeableString {
  explicit MergeableString(const eld::MergeableString *S);
  /// return the null-terminated string that this MergeableString represents.
  const char *getString() const;
  /// return the size of the string, including the null terminating character.
  uint32_t getSize() const;
  /// return the offset within the input section where this string is located.
  uint32_t getInputOffset() const;
  /// return the output offset assigned to this string, or UINT_MAX if the
  /// output offset has not yet been assigned, or this string is a duplicate of
  /// another and exluded from the output.
  uint32_t getOutputOffset() const;
  /// return true if this string has been assigned an output offset. A value of
  /// false means that the linker has not yet assigned an offset to the
  /// section containing the string, or the string was merged with another
  /// string.
  bool hasOutputOffset() const;
  /// return true if this string is the duplicate of another string and exluded
  /// from the output.
  bool isMerged() const;

private:
  const eld::MergeableString *String = nullptr;
};

/// LinkerScriptRule represents an output section command in a linker script.
/// For example:
/// \code
/// section {
///   .text : {
///     *(.text)
///     LONG(1)
///     _etext = .;
///   }
/// }
/// \endcode
/// In this example, \em *(.text), \em LONG(1), and \em _etext=1, all
/// are output section commands. They are all represented as
/// LinkerScriptRule objects. Here \em .text is the corresponding output
/// section of the rule.
///
/// A LinkerScriptRule object can be used to inspect, and modify linker
/// rules. Rules determine the output image layout and it's contents.
///
/// LinkerScriptRule is a wrapper for an internal RuleContainer class.
/// If two LinkerScriptRule objects have the same underlying RuleContainer
/// object, then both the objects represent the same rule.
struct DLL_A_EXPORT LinkerScriptRule final {

  enum State : uint8_t { Empty, NoChunk, DuplicateChunk, NotEmpty, Ok };

  LinkerScriptRule() : m_RuleContainer(nullptr) {}

  LinkerScriptRule(eld::RuleContainer *I) : m_RuleContainer(I) {}

  /// Returns true if the sections matched by the linker script rule can be
  /// moved to another output section; Otherwise returns false.
  bool canMoveSections() const;

  /// Returns true if this rule is inserted by the linker; Otherwise
  /// returns false.
  bool isLinkerInsertedRule() const;

  /// Returns true if the rule corresponds to KEEP sections.
  bool isKeep() const;

  /// Returns the hash of the linkerscript rule
  std::optional<uint64_t> getHash() const;

  /// Returns the input section specification of the linker script rule.
  ///
  /// \note An Input Section description is an internal representation of
  /// a linker script rule.
  plugin::Script::InputSectionSpec getInputSectionSpec() const;

  /// Returns the output section corresponding to the rule.
  plugin::OutputSection getOutputSection() const;

  /// Returns true if the rule has any expression; Otherwise returns false.
  ///
  /// Expressions are symbol assignments.
  bool hasExpressions() const;

  /// Returns true if the expression modifies the location counter ('.');
  /// Otherwise returns false.
  bool doesExpressionModifyDot() const;

  /// Return the rule as a string for diagnostics.
  std::string asString() const;

  // Add/Remove/Update chunks for any section updates.

  /// Add chunk C to this rule. All properties specified by the rule will
  /// be applied to the chunk C.
  ///
  /// \note A chunk can be a part of only one rule. Therefore, on using this
  /// function, the chunk C is removed from any other rule that it was part of.
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  DEPRECATED void addChunk(plugin::Chunk C);

  /// Remove chunk C from this rule. No property specified by the rule
  /// will be applied to the chunk C.
  ///
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  DEPRECATED void removeChunk(plugin::Chunk C);

  /// Remove all the existing chunks and sections from the rule,
  /// and add all the chunks in sequence C to the rule.
  ///
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  DEPRECATED void updateChunks(std::vector<plugin::Chunk> C);

  /// If verify is true, then perform verification while adding the chunk;
  /// Otherwise simply add the chunk and returns the State::Ok.
  ///
  /// Verification makes sure a chunk is not being added multiple times
  /// to the rule. Verification step checks for duplication, and
  /// if the rule contains the chunk that is to be added, then the function
  /// returns State::DuplicateChunk and does not add the chunk to the rule.
  ///
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  ///
  /// \param C chunk to be added
  /// \param Verify Specifies if verification should be performed or not.
  /// \return State::Ok if the verification succeeds; Otherwise return a state
  /// that describes the verification failure.
  DEPRECATED State addChunk(plugin::Chunk C, bool Verify);

  /// If verify is true, then perform verification while removing the chunk;
  /// Otherwise simply remove the chunk and returns the State::Ok.
  ///
  /// Verfication makes sure the rule indeed contains the chunk that is
  /// to be removed.
  ///
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  ///
  /// \param C chunk to be removed
  /// \param Verify Specifies if verification should be performed or not.
  /// \return State::Ok if the verification succeeds; Otherwise return a state
  /// that describes the verification failure.
  DEPRECATED State removeChunk(plugin::Chunk C, bool Verify);

  /// If verify is true, then perform verification while updating chunks;
  /// Otherwise simply update chunks and returns the State::Ok.
  ///
  /// Verification make sure that initially the rule contained no chunks,
  /// and no duplicate chunks are being added.
  ///
  /// \note This function should only be used when the linker state is
  /// BeforeLayout. Behavior of using this function in other linker
  /// states is undefined.
  ///
  /// \param C chunk to be removed
  /// \param Verify Specifies if verification should be performed or not.
  /// \return State::Ok if the verification succeeds; Otherwise return a state
  /// that describes the verification failure.
  DEPRECATED State updateChunks(std::vector<plugin::Chunk> C, bool Verify);

  /// Return chunks associated with this linker script rule.
  std::vector<plugin::Chunk> getChunks() const;

  /// Return sections associated with the linker script rule.
  std::vector<plugin::Section> getSections();

  /// Return all sections matched by this rule including garbage collected
  /// sections.
  std::vector<plugin::Section> getMatchedSections() const;

  /// Get all the Chunks in the rule that correspond to Section S.
  std::vector<plugin::Chunk> getChunks(plugin::Section S);

  /// Return the corresponding RuleContainer of the linker script rule.
  eld::RuleContainer *getRuleContainer() const { return m_RuleContainer; }

  /// Returns true if the object has a corresponding non-null RuleContainer
  /// object; Otherwise returns false.
  operator bool() const { return m_RuleContainer != nullptr; }

  //
  //  Operators
  //

  /// Returns true if A and B have different underlying RuleContainer objects.
  bool operator()(const LinkerScriptRule &A, const LinkerScriptRule &B) const {
    return A.m_RuleContainer != B.m_RuleContainer;
  }

  /// Pointer comparision between the underlying RuleContainer handle of
  /// LHS and RHS.
  bool operator<(const LinkerScriptRule &RHS) const {
    return m_RuleContainer < RHS.m_RuleContainer;
  }

  /// Pointer comparision between the underlying RuleContainer handle of
  /// LHS and RHS.
  bool operator>(const LinkerScriptRule &RHS) const {
    return m_RuleContainer > RHS.m_RuleContainer;
  }

  /// Returns true if LHS and RHS have the same RuleContainer objects.
  bool operator==(const LinkerScriptRule &RHS) const {
    return m_RuleContainer == RHS.m_RuleContainer;
  }

private:
  eld::RuleContainer *m_RuleContainer;
};
/// Segment is a handler for an ELF Segment. A Segment object can be used
/// to inspect Segment properties and their associated OutputSections.
struct DLL_A_EXPORT Segment final {

  explicit Segment(eld::ELFSegment *S) : S(S) {}

  /// \return name of this segment
  std::string getName() const;

  /// \return Type of this segment (ELF p_type)
  uint32_t getType() const;

  /// \return Byte offset from the beginning of the file to where this segment
  /// starts (ELF p_offset)
  uint64_t getOffset() const;

  /// \return Virtual address of this segment (ELF p_vaddr)
  uint64_t getVirtualAddress() const;

  /// \return Physical address of this segment (ELF p_paddr)
  uint64_t getPhysicalAddress() const;

  /// \return Size in bytes of the file image of this segment (ELF p_filesz)
  uint64_t getFileSize() const;

  /// \return Size in bytes of the memory image of this segment (ELF p_memsz)
  uint64_t getMemorySize() const;

  /// \return Segment flags (ELF p_flags)
  uint32_t getSegmentFlags() const;

  /// \return Alignment of this segment. A value of 0 or 1 indicates no
  /// alignment requirement. (ELF p_align)
  uint64_t getPageAlignment() const;

  /// \return true if this segment is loaded (PT_LOAD)
  bool isLoadSegment() const;

  /// \return true if this segment is TLS (PT_TLS)
  bool isTLSSegment() const;

  /// \return true if this segment is dynamic (PT_DYNAMIC)
  bool isDynamicSegment() const;

  /// \return true if this segment is relocation read only (PT_GNU_RELRO)
  bool isRelROSegment() const;

  /// \return true if this segment is a note segment (PT_NOTE)
  bool isNoteSegment() const;

  /// \return true if this segment is a null segment (PT_NULL)
  bool isNullSegment() const;

  /// \return the maximum alignment of sections in this segment.
  uint32_t getMaxSectionAlign() const;

  /// \return The output sections contained in this segment
  eld::Expected<std::vector<OutputSection>>
  getOutputSections(const LinkerWrapper &LW) const;

  /// \return the internal representation for this segment.
  const eld::ELFSegment *getSegment() const { return S; }

private:
  const eld::ELFSegment *S;
};

/// A stub is a jump-pad to a direct function call.
///
/// It is typically used as an indirect-call for far-function calls.
/// Far-function calls are function calls where the function address
/// cannot be represented in the default function call instruction.
class DLL_A_EXPORT Stub {
public:
  explicit Stub(const eld::BranchIsland *pBI) : BI(pBI) {}

  /// Returns the target symbol for the stub.
  ///
  /// If the stub is a jump-pad to 'foo()' function call, then
  /// 'foo' is the target symbol.
  eld::plugin::Symbol getTargetSymbol() const;

  /// Returns the stub symbol.
  ///
  /// A stub symbol is a linker-generated symbols and it points to the
  /// address that contains direct function call to the stub's target
  /// symbol.
  eld::plugin::Symbol getStubSymbol() const;

private:
  const eld::BranchIsland *BI;
};

/// OutputSection is a handler for an output section. An OutputSection object
/// can be used to inspect an output section, and get it's associated linker
/// script rules.
///
/// It is a wrapper for an internal OutputSectionEntry class. If
/// two OutputSection objects points to the same underlying
/// OutputSectionEntry object, then they both represent the same
/// output section.
struct DLL_A_EXPORT OutputSection final {
  explicit OutputSection(eld::OutputSectionEntry *O) : m_OutputSection(O) {}

  /// Returns the name of the output section.
  ///
  /// If the object is an empty handler, then returns an empty string.
  std::string getName() const;

  /// Returns the alignment of the output section.
  uint64_t getAlignment() const;

  /// Returns the section attribute flags.
  /// Each bit of flag defines some attribute. If a flag bit is set, then
  /// the attribute which corresponds to that bit is on for the section.
  /// Otherwise, the attribute is off and does not apply. Examples of section
  /// attributes: \em SHF_WRITE, \em SHF_ALLOC, \em SHF_EXECINSTR, and \em
  /// SHF_MASKPROC etc.
  uint64_t getFlags() const;

  /// Returns the section semantics type.
  /// Section type specifies the section semantics e.g. SHT_PROGBITS,
  /// SHT_NOBITS, SHT_NOTE, etc.
  uint64_t getType() const;

  /// Returns the Index of the output section.
  /// If the object is an empty handler, then returns 0.
  // TODO: What is index of the output section? Why are the index useful from
  // the user's perspective?
  uint64_t getIndex() const;

  /// Returns the number of bytes that the section occupies in the file.
  uint64_t getSize() const;

  /// Returns all linker script rules for the output section.
  std::vector<plugin::LinkerScriptRule> getLinkerScriptRules();

  /// Returns a pointer to the underlying OuputSectionEntry object.
  /// This can be used as a unique identifier for the output section.
  const eld::OutputSectionEntry *getOutputSection() const {
    return m_OutputSection;
  }

  /// \return The load segment for this output section, if one exists. This API
  /// should only be called in CreatingSections or AfterLayout linker states.
  eld::Expected<std::optional<Segment>>
  getLoadSegment(const LinkerWrapper &LW) const;

  /// \return All segments this OutputSection belongs to. This API should only
  /// be called in CreatingSections or AfterLayout linker states.
  eld::Expected<std::vector<Segment>>
  getSegments(const LinkerWrapper &LW) const;

  /// Returns a pointer to the underlying OutputSectionEntry object.
  /// This can be used as a unique identifier for the output section.
  eld::OutputSectionEntry *getOutputSection() { return m_OutputSection; }

  /// Returns the virtual address of the output section.
  eld::Expected<uint64_t> getVirtualAddress(const LinkerWrapper &) const;

  /// Returns the physical address of the output section.
  eld::Expected<uint64_t> getPhysicalAddress(const LinkerWrapper &) const;

  /// Returns true if the object represents a non-null output section.
  operator bool() const { return m_OutputSection != nullptr; }

  //
  // Operators
  //

  /// Returns true if A and B represents different output sections.
  bool operator()(const OutputSection &A, const OutputSection &B) const {
    return A.m_OutputSection != B.m_OutputSection;
  }

  /// Pointer comparison between the underlying OutputSectionEntry handles of
  /// LHS and RHS.
  bool operator<(const OutputSection &RHS) const {
    return m_OutputSection < RHS.m_OutputSection;
  }

  /// Pointer comparison between the underlying OutputSectionEntry handles of
  /// LHS and RHS.
  bool operator>(const OutputSection &RHS) const {
    return m_OutputSection > RHS.m_OutputSection;
  }

  /// Returns true if both the LHS and RHS objects represents the same
  /// output sections.
  bool operator==(const OutputSection &RHS) const {
    return m_OutputSection == RHS.m_OutputSection;
  }

  /// Return the offset of this OutputSection, or an error if it has not been
  /// assigned one.
  eld::Expected<uint64_t> getOffset() const;

  /// Set the output offset of this section, or error.
  eld::Expected<void> setOffset(uint64_t Offset, const LinkerWrapper &LW);

  /// Returns true if this section is of type SHT_NOBITS (the section occupies
  /// no file space).
  bool isNoBits() const;

  bool isEmpty() const;

  /// Returns true if the section is being discarded.
  bool isDiscard() const;

  /// Returns true if the section contains code with SHF_EXECINSTR flag set.
  bool isCode() const;

  /// does this section have the SHF_ALLOC flag
  bool isAlloc() const;

  /// Returns a list of all stubs created for this output section.
  std::vector<Stub> getStubs() const;

  /// Plugin P assumes repsonsibility for this section. Some layout
  /// warnings/errors will be ignored for such sections (e.g. "loadable section
  /// `foo` not in any load segment"). This API may only be used in
  /// `CreatingSegments` state.
  Expected<void> setPluginOverride(Plugin *P, const LinkerWrapper &LW);

private:
  // Internal Data structure.
  eld::OutputSectionEntry *m_OutputSection = nullptr;
};

/// Section is a handler for an ELF input section. A Section object can be used
/// to inspect section properties, chunks and symbols. It can also be
/// used to discard a section.
///
/// Section is a wrapper for an internal Section class. If two objects
/// have the same underlying Section object, then they both represents the
/// same section.
struct DLL_A_EXPORT Section {
  // TODO: What do each of these types means? Where is SectionType used?
  enum SectionType : uint8_t { Default, Padding };

  explicit Section(eld::Section *S = nullptr,
                   SectionType T = SectionType::Default)
      : m_Section(S), Type(T) {}

  // Section flags
  static const size_t SHF_WRITE;
  static const size_t SHF_ALLOC;
  static const size_t SHF_EXECINSTR;
  static const size_t SHF_MERGE;
  static const size_t SHF_STRINGS;
  static const size_t SHF_GNU_RETAIN;

  // Section types
  static const size_t SHT_NULL;
  static const size_t SHT_PROGBITS;
  static const size_t SHT_NOTE;
  static const size_t SHT_NOBITS;

  /// Returns the name of the section.
  /// If the object is an empty handler, then returns an empty string.
  std::string getName() const;

  /// Returns the type of the input section.
  /// If the object is an empty handler, then returns SectionType::Default.
  SectionType getType() const { return Type; }

  /// Returns the InputFile of the input section.
  /// If the object is an empty handler, then returns an empty InputFile object.
  plugin::InputFile getInputFile() const;

  /// Returns index of the section in the input file.
  /// If the object is an empty handler, then returns 0xFFFFFFFF.
  uint32_t getIndex() const;

  /// Returns the size of the section.
  uint32_t getSize() const;

  /// Returns true if the section is being discarded.
  bool isDiscarded() const;

  /// Returns true if the section is garbage collected.
  bool isGarbageCollected() const;

  /// Returns the alignment of the section if the object.
  uint32_t getAlignment() const;

  /// Returns true if the section name matches the pattern Pattern.
  ///
  /// \note Here, pattern matching follows unix glob style pattern matching.
  ///
  /// \param Pattern glob pattern
  /// \returns true if the section name matches the pattern Pattern;
  /// Otherwise false.
  bool matchPattern(const std::string &Pattern) const;

  /// Returns a sequence of symbols contained by this section.
  std::vector<plugin::Symbol> getSymbols() const;

  /// Returns a sequence of chunks contained by this section.
  std::vector<plugin::Chunk> getChunks() const;

  /// Discard the section from the output object file.
  ///
  /// \warning This is a dangerous function, if not used carefully,
  /// it can discard important information, that can result in invalid
  /// output object file.
  /// TODO: What exactly discarding a section means?
  void markAsDiscarded();

  /// Returns true if the section is stored in the image. In ELF, such sections
  /// are of the PROGBITS type.
  bool isProgBits() const;

  /// Returns true if the section is not stored in the image. In ELF, such
  /// sections are of the NOBITS type.
  bool isNoBits() const;

  /// Returns true if the section contains code. More precisely, it returns
  /// true if the section has SHF_EXECINSTR flag set.
  bool isCode() const;

  /// Returns true if the section is allocated in memory. In ELF, such sections
  /// have the ALLOC attribute.
  bool isAlloc() const;

  /// Returns true if the section is writable. In ELF, such sections have the
  /// WRITE attribute.
  bool isWritable() const;

  bool isNote() const;

  /// Return a pointer to the underlying section handle.
  ///
  /// This can be used as a unique identifier for the section.
  eld::Section *getSection() const { return m_Section; }

  /// Returns the output section for this section.
  plugin::OutputSection getOutputSection() const;

  /// Return the linker script rule that is matched for this section.
  LinkerScriptRule getLinkerScriptRule() const;

  /// Returns a sequence of sections that are dependent on this
  /// section.
  std::vector<plugin::Section> getDependentSections() const;

  std::optional<uint64_t> getOffset() const;

  std::optional<uint32_t> getEntrySize() const;

  /// Returns true if the object represents a non-null section.
  operator bool() const { return m_Section != nullptr; }

  //
  // Operators
  //

  /// Returns true if A and B represents different sections.
  bool operator()(const Section &A, const Section &B) const {
    return A.m_Section != B.m_Section;
  }

  /// Pointer comparision between the underlying section handlers.
  bool operator<(const Section &RHS) const { return m_Section < RHS.m_Section; }

  /// Pointer comparision between the underlying section handlers.
  bool operator>(const Section &RHS) const { return m_Section > RHS.m_Section; }

  /// Return true if both the lhs and rhs represents the same section.
  bool operator==(const Section &RHS) const {
    return m_Section == RHS.m_Section;
  }

protected:
  // Internal Data structure.
  eld::Section *m_Section;
  SectionType Type;
};

/// Block represents output sections and their content.
// TODO:
struct DLL_A_EXPORT Block final {
  Block() : Data(nullptr), Size(0), Address(0), Alignment(1) {}
  const uint8_t *Data; ///< Data passed to the plugin
  uint32_t Size;       ///< Size of the data in bytes.
  uint32_t Address;    ///< Address of the data
  uint32_t Alignment;  ///< Alignment of the data
  std::string Name;    ///< Name of the block
};

/// Use represents references from a chunk. In ELD terminology, source
/// address and source chunk of the Use are the places where relocation has to
/// be performed. And target address and target chunk are the places which
/// contain the relocation symbol definition. Use can be used to inspect symbol
/// references.
///
/// Use is a wrapper class for an internal relocation class.
/// If two Use objects have the same underlying relocation objects,
/// then the two Use objects represents the same Use.
struct DLL_A_EXPORT Use final {
  /// Symbol status.
  enum Status : uint8_t { Ok, SymbolDoesNotExist, Error };
  explicit Use(eld::Relocation *R) : m_Relocation(R) {}

  /// If the Use represents a valid relocation, then return the symbol name
  /// described by this Use; Otherwise return an empty string.
  /// The symbol name is returned as how it is recorded in the input
  /// ELF file.
  std::string getName() const;

  /// If the Use represents a valid relocation, then returns the chunk that
  /// defines the symbol represented by this Use; Otherwise returns an
  /// empty Chunk object.
  /// DEPRECATED in favor of Use::getTargetChunk
  DEPRECATED Chunk getChunk() const;

  /// If the Use represents a valid relocation, then returns the chunk that
  /// contain the symbol reference represented by this Use; Otherwise
  /// returns an empty Chunk object.
  Chunk getSourceChunk() const;

  /// If the Use represents a valid relocation, then returns the corresponding
  /// symbol info of the relocation; Otherwise returns a null Symbol object.
  plugin::Symbol getSymbol() const;

  /// Returns a pointer to the underlying relocation object.
  eld::Relocation *getRelocation() const { return m_Relocation; }

  /// Reset the symbol info to a different symbol info object.
  ///
  /// \param S Symbol that should replace the already existing symbol.
  /// \returns Status of the replacement.
  Use::Status resetSymbol(plugin::Symbol S);

  /// If the Use has a non-null relocation handle, then returns the
  /// relocation type; Otherwise returns UINT32_MAX.
  uint32_t getType() const;

  /// If the Use represents a valid relocation, then returns the source
  /// chunk offset in it's corresponding section; Otherwise returns -1.
  off_t getOffsetInChunk() const;

  // Get the address of the Use.
  /// If the Use represents a valid relocation; then return the address
  /// of the Use -- target address of relocation; Otherwise returns
  /// UINT64_MAX.
  size_t getSourceAddress() const;

  /// If the Use represents a valid relocation; then returns the chunk that
  /// defines the symbol represented by this Use; Otherwise returns an
  /// empty Chunk object.
  // FIXME: Same as the function getChunk
  plugin::Chunk getTargetChunk() const;

  /// If the Use has a non-null relocation handle; then returns the chunk
  /// that defines the symbol represented by this Use;
  // FIXME: null relocation handle return -1, whereas empty relocation object
  // returns 0 in this case. Thus, there is no unique return value for the case
  // when there is no appropriate offset.
  off_t getTargetChunkOffset() const;

  /// Returns true if the Use has a non-null relocation handle.
  operator bool() const { return m_Relocation != nullptr; }

  /// Returns true if the Use A and B represent different Uses.
  bool operator()(const Use &A, const Use &B) const {
    return A.m_Relocation != B.m_Relocation;
  }

  /// Pointer comparison between relocation handles of LHS and RHS.
  bool operator<(const Use &RHS) const {
    return m_Relocation < RHS.m_Relocation;
  }

  /// Pointer comparison between relocation handles of LHS and RHS.
  bool operator>(const Use &RHS) const {
    return m_Relocation > RHS.m_Relocation;
  }

  /// Returns true if the Use LHS and RHS represents the same use.
  bool operator==(const Use &RHS) const {
    return m_Relocation == RHS.m_Relocation;
  }

private:
  /// Internal Data structure.
  eld::Relocation *m_Relocation;
};

/// Symbol is a handler for a linker symbol. Symbol can be used to
/// inspect a symbol.
///
/// Symbol is a wrapper for the internal ResolveInfo class. If two
/// Symbol objects have the same underlying ResolveInfo object, then
/// they both represent the same symbol.
struct DLL_A_EXPORT Symbol final {

  enum Binding { Global = 0, Weak = 1, Local = 2 };
  enum Kind { Undefined = 0, Define = 1, Common = 2 };
  enum Visibility { Default = 0, Internal = 1, Hidden = 2, Protected = 3 };

  explicit Symbol(eld::ResolveInfo *S) : m_Symbol(S) {}

  /// Returns the chunk that defines the symbol.
  ///
  /// If the object is an empty handler, then returns an empty Chunk object.
  Chunk getChunk() const;

  /// Returns the symbol name.
  ///
  /// If the object is an empty handler, then returns an empty string.
  std::string getName() const;

  /// Returns true if the symbol is local.
  bool isLocal() const;

  /// Returns true if the symbol is weak.
  bool isWeak() const;

  /// Returns true if the symbol is global.
  bool isGlobal() const;

  /// Returns true if the symbol is undefined.
  bool isUndef() const;

  /// Returns true if the symbol is common.
  bool isCommon() const;

  /// Returns true if the symbol is section.
  bool isSection() const;

  /// Returns true if the symbol is function.
  bool isFunction() const;

  /// Returns true if the symbol is object.
  bool isObject() const;

  /// Returns true if the symbol is File.
  bool isFile() const;

  /// Returns true if the symbol is NoType.
  bool isNoType() const;

  /// Returns true if the symbol got garbage collected.
  bool isGarbageCollected() const;

  /// Returns the resolved path of a Symbol.
  std::string getResolvedPath() const;

  /// Returns the symbol size.
  ///
  /// A symbol size is the number of bytes contained in the object represented
  /// by the symbol.
  uint32_t getSize() const;

  /// Returns the symbol value.
  ///
  /// Please note that:
  // - In relocatable files, symbol value holds alignment constraints for common
  // symbols.
  // - In relocatable files, symbol value holds a section offset for a defined
  // symbol.
  // - In executable and shared object files, symbol value holds a virtual
  // address.
  size_t getValue() const;

  /// Get the Symbol Address. Most times the symbol value
  /// will be equal to the address of the symbol. If there
  /// is a need to access the address of the symbol before
  /// a symbol gets finalized, getAddress can be utilized.
  // TODO: In which cases would the symbol value and address be different?
  size_t getAddress() const;

  /// Returns the symbol's offset in chunk.
  off_t getOffsetInChunk() const;

  /// Return a pointer to the underlying ResolveInfo object.
  /// This can be used as a unique identifier for the symbol.
  eld::ResolveInfo *getSymbol() const { return m_Symbol; }

  uint64_t getSymbolIndex() const;

  plugin::InputFile getInputFile();

  /// Conversion Operator
  operator bool() const { return m_Symbol != nullptr; }

  //
  // Operators
  //
  /// Returns true if A and B represents different symbols.
  bool operator()(const Symbol &A, const Symbol &B) const {
    return A.m_Symbol != B.m_Symbol;
  }

  /// Pointer comparison between the underlying ResolveInfo handles of LHS
  /// and RHS.
  bool operator<(const Symbol &RHS) const { return m_Symbol < RHS.m_Symbol; }

  /// Pointer comparison between the underlying ResolveInfo handles of LHS
  /// and RHS.
  bool operator>(const Symbol &RHS) const { return m_Symbol > RHS.m_Symbol; }

  /// Returns true if LHS and RHS represents the same symbol.
  bool operator==(const Symbol &RHS) const { return m_Symbol == RHS.m_Symbol; }

private:
  /// Internal Data structure
  eld::ResolveInfo *m_Symbol;
};

/// INIFile represents an ini configuration file. It provides APIs
/// to easily inspect and modify the INI configuration.
struct DLL_A_EXPORT INIFile final {

  enum ErrorCode { Success, FileDoesNotExist, ReadError, WriteError };

  /// Initialize an INIFile
  explicit INIFile(const std::string FileName);

  INIFile &operator=(const INIFile &File) = delete;

  INIFile(const INIFile &) = delete;

  INIFile(INIFile &&other) noexcept;

  INIFile &operator=(INIFile &&other) noexcept;

  INIFile();

  ~INIFile();

  /// Add an empty section to this file if does not currently exist
  void addSection(const std::string &section);

  /// Insert a key-value pair into a given section
  /// If the section does not exist, it is automatically created.
  /// \param section Name of the section in which key-value pair has to be
  /// inserted
  /// \param K Key
  /// \param V Value
  void insert(const std::string &section, const std::string &K,
              const std::string &V);

  /// \returns true if a section with name sectionName is found in this file;
  /// Otherwise false.
  bool containsSection(const std::string &sectionName) const;

  /// \return true if the section `sectionName` contains the key named `key';
  /// Otherwise false.
  bool containsItem(const std::string &sectionName,
                    const std::string &key) const;

  /// \returns vector of all section names in this file
  std::vector<std::string> getSections() const;

  /// \returns A vector of all key/value pairs in a given section of this file
  /// If the provided section does not exist or is empty,
  /// an empty vector is returned
  /// \param Section Name of the section of the file, minus any brackets
  std::vector<std::pair<std::string, std::string>>
  getSection(const std::string Section) const;

  /// Get the value of an item in a given section of this file
  /// \param Section Name of the section, without brackets
  /// \param Item Name of an item within the section
  /// \return Value associated with Item in Section, if it exists
  /// If provided Section or Item does not exist, an empty string is returned
  std::string getValue(const std::string Section, const std::string Item);

  /// \return INIFile::ErrorCode depending on the last error
  ErrorCode getErrorCode() { return m_LastError; }

  /// Set last error.
  void setLastError(ErrorCode e) { m_LastError = e; }

  /// Return last error as string.
  std::string getLastErrorAsString();

  /// return true if file is non-empty.
  operator bool() const;

  /// Return the underlying INI Reader pointer for the file.
  eld::INIReader *getReader() const { return m_Reader; }

  /// Creates a INIFile object for filename. INIFile object can be used
  /// to easily perform suitable read and write operations to the INI file.
  ///
  /// \returns eld::Expected<INIFile> contains INIFile if initialisation of
  /// INIFile object was successful; Otherwise it contains a diagnostic object
  /// describing the error.
  static eld::Expected<INIFile> Create(const std::string &filename);

private:
  explicit INIFile(const std::string &filename, eld::Expected<bool> &exp);

  eld::INIReader *m_Reader = nullptr;
  ErrorCode m_LastError;
};

/// plugin::MemoryBuffer can be used to hold and retrieve uint8_t* data.
/// E.g. Adding data to a tar file using the plugin::TarWriter's
/// addBufferToTar(eld::Expected<plugin::MemoryBuffer> buffer) function.
class DLL_A_EXPORT MemoryBuffer final {
private:
  std::unique_ptr<eld::MemoryArea> getBuffer();

public:
  explicit MemoryBuffer(std::unique_ptr<eld::MemoryArea> Buf);

  /// Creates a plugin::MemoryBuffer for a given uint8_t* data and returns
  /// eld::Expected<MemoryBuffer>
  /// \param std::string Name of data
  /// \param uint8_t* Data to be stored
  /// \param size_t Length of data
  /// \param bool isNullTerminated Specifies if data is null terminated
  /// \returns eld::Expected<MemoryBuffer>
  static eld::Expected<MemoryBuffer> getBuffer(const std::string &Name,
                                               const uint8_t *Data,
                                               size_t Length,
                                               bool isNullTerminated = false);

  // Disable copy operations.
  MemoryBuffer(const MemoryBuffer &) = delete;
  MemoryBuffer &operator=(const MemoryBuffer &) = delete;

  // Movable.
  MemoryBuffer(MemoryBuffer &&) noexcept;
  MemoryBuffer &operator=(MemoryBuffer &&) noexcept;

  /// \returns buffer name.
  std::string getName() const;

  /// \returns buffer contents.
  const uint8_t *getContent() const;

  /// \returns size_t size of buffer.
  size_t getSize() const;

  ~MemoryBuffer();

private:
  std::unique_ptr<eld::MemoryArea> m_Buffer;
  friend class plugin::TarWriter;
};

/// InputFile represents an input file. Input file can be an object file,
/// a linker script, or anything else that the the linker can work
/// with. An InputFile object can be used to inspect input file properties,
/// and contents.
struct DLL_A_EXPORT InputFile {
  explicit InputFile(eld::InputFile *I) : m_InputFile(I) {}

  /// Returns the file name of the input file if the object has an
  /// input file; Otherwise returns an empty string.
  std::string getFileName() const;

  /// Returns true if the object has an input file; Otherwise returns false.
  bool hasInputFile() const;

  /// Returns true if the input is an archive member; Otherwise returns false.
  bool isArchive() const;

  /// Returns true if the input is LLVM bitcode; Otherwise returns false.
  bool isBitcode() const;

  /// If the input file is an archive member, return the member name;
  /// Otherwise return an empty string.
  std::string getMemberName() const;

  /// Returns a sequence of all the symbols that are defined in this file.
  std::vector<plugin::Symbol> getSymbols() const;

  /// Returns a sequence of all the sections in the input file.
  std::vector<plugin::Section> getSections() const;

  /// Get section at index i
  std::optional<plugin::Section> getSection(uint64_t i) const;

  /// Returns true if the object has an input file.
  operator bool() const { return m_InputFile != nullptr; }

  /// Returns a pointer to the data of the input file.
  const char *getMemoryBuffer() const;

  std::string_view slice(uint32_t offset, uint32_t size) const;

  /// Returns the input file size.
  size_t getSize() const;

  /// Returns true if the input file is an internal input file.
  bool isInternal() const;

  /// Returns ture if the input file is an archive member.
  bool isArchiveMember() const;

  /// Returns the archive file that contains the input if the input is an
  /// archive member; Otherwise returns std::nullopt.
  std::optional<plugin::InputFile> getArchiveFile() const;

  //
  // Operators
  //

  /// Returns true if A and B represents different input files.
  bool operator()(const InputFile &A, const InputFile &B) const {
    return A.m_InputFile != B.m_InputFile;
  }

  /// Pointer comparison between the underlying input file handles of LHS
  /// and RHS.
  bool operator<(const InputFile &RHS) const {
    return m_InputFile < RHS.m_InputFile;
  }

  /// Pointer comparison between the underlying input file handles of LHS
  /// and RHS.
  bool operator>(const InputFile &RHS) const {
    return m_InputFile > RHS.m_InputFile;
  }

  /// Returns true if both LHS and RHS represents the same input file.
  bool operator==(const InputFile &RHS) const {
    return m_InputFile == RHS.m_InputFile;
  }

  bool operator!=(const InputFile &rhs) const { return !(*this == rhs); }

  /// Returns the underlying input file handler.
  /// This can be used as a unique identifier for the input file.
  eld::InputFile *getInputFile() const { return m_InputFile; }

  /// Return the input ordinal which is the order in which the linker visited
  /// this input file
  uint32_t getOrdinal() const;

  /// TODO
  std::string decoratedPath() const;

  std::string getRealPath() const;

private:
  eld::InputFile *m_InputFile;
};

/// An interface to represent a bitcode file to retrieve the LTO Input file.
struct DLL_A_EXPORT BitcodeFile final : public InputFile {

  explicit BitcodeFile(eld::BitcodeFile &F);

  eld::BitcodeFile &getBitcodeFile() const;

  llvm::lto::InputFile &getInputFile() const;
  bool findIfKeptComdat(unsigned index) const;
};

struct DLL_A_EXPORT DynamicLibrary final {
  void *Handle;
  std::string Path;
};

/// PluginData provide a way for storing data and communicating between plugins.
// TODO: Add a demo that shows the usage of PluginData.
struct DLL_A_EXPORT PluginData final {
  explicit PluginData(eld::PluginData *D) : m_Data(D) {}

  void *getData() const;

  uint32_t getKey() const;

  std::string getAnnotation() const;

  /// Conversion Operator
  operator bool() const { return m_Data != nullptr; }

  ///
  /// Operators
  ///
  bool operator()(const PluginData &A, const PluginData &B) const {
    return A.m_Data != B.m_Data;
  }

  bool operator<(const PluginData &RHS) const { return m_Data < RHS.m_Data; }

  bool operator>(const PluginData &RHS) const { return m_Data > RHS.m_Data; }

  bool operator==(const PluginData &RHS) const { return m_Data == RHS.m_Data; }

private:
  eld::PluginData *m_Data;
};

/// Plugin Profiling support
struct DLL_A_EXPORT AutoTimer final {
  explicit AutoTimer(llvm::Timer *T);

  /// Start Timer
  void start();

  /// End Timer
  void end();

  ~AutoTimer();

  /// Conversion Operator
  operator bool() const { return m_Timer != nullptr; }

  ///
  /// Operators
  ///
  bool operator()(const AutoTimer &A, const AutoTimer &B) const {
    return A.m_Timer != B.m_Timer;
  }

  bool operator<(const AutoTimer &RHS) const { return m_Timer < RHS.m_Timer; }

  bool operator>(const AutoTimer &RHS) const { return m_Timer > RHS.m_Timer; }

  bool operator==(const AutoTimer &RHS) const { return m_Timer == RHS.m_Timer; }

private:
  llvm::Timer *m_Timer;
};

struct DLL_A_EXPORT Timer final {
  Timer(llvm::Timer *T);

  /// Start Timer
  void start();

  /// End Timer
  void end();

  ~Timer();

  /// Conversion Operator
  operator bool() const { return m_Timer != nullptr; }

  ///
  /// Operators
  ///
  bool operator()(const Timer &A, const Timer &B) const {
    return A.m_Timer != B.m_Timer;
  }

  bool operator<(const Timer &RHS) const { return m_Timer < RHS.m_Timer; }

  bool operator>(const Timer &RHS) const { return m_Timer > RHS.m_Timer; }

  bool operator==(const Timer &RHS) const { return m_Timer == RHS.m_Timer; }

private:
  llvm::Timer *m_Timer;
};

/// RelocationHandler handles relocations. It can be
/// to inspect and compare relocations.
// FIXME: This class do not provide much functionality to handle
// relocations.
struct DLL_A_EXPORT RelocationHandler final {

  RelocationHandler(eld::Relocator *R);

  /// Get Relocation Type for a relocation.
  uint32_t getRelocationType(std::string Name) const;

  /// Get the Relocation name from its type.
  std::string getRelocationName(uint32_t Type) const;

  ~RelocationHandler();

  /// Conversion Operator
  operator bool() const { return Relocator != nullptr; }

  ///
  /// Operators
  ///
  bool operator()(const RelocationHandler &A,
                  const RelocationHandler &B) const {
    return A.Relocator != B.Relocator;
  }

  bool operator<(const RelocationHandler &RHS) const {
    return Relocator < RHS.Relocator;
  }

  bool operator>(const RelocationHandler &RHS) const {
    return Relocator > RHS.Relocator;
  }

  bool operator==(const RelocationHandler &RHS) const {
    return Relocator == RHS.Relocator;
  }

private:
  eld::Relocator *Relocator;
};

class DLL_A_EXPORT InputSymbol final {
public:
  InputSymbol();

  InputSymbol(eld::LDSymbol *sym, std::string_view symName,
              std::unique_ptr<eld::SymbolInfo> symInfo);

  InputSymbol(const InputSymbol &);

  InputSymbol &operator=(const InputSymbol &);

  ~InputSymbol();

  std::string_view getName() const;

  InputFile getInputFile() const;

  uint64_t getSymbolIndex() const;

  bool isUndef() const;

  bool isCommon() const;

  bool isLocal() const;

  eld::ResolveInfo *getResolveInfo();

  eld::LDSymbol *getInputSymbol() { return Sym; }

private:
  eld::LDSymbol *Sym;
  std::string_view SymName;
  std::unique_ptr<eld::SymbolInfo> SymInfo;
};

/// LinkerConfig houses functions for plugins to access linker configurations
/// and options.
struct DLL_A_EXPORT LinkerConfig {
  explicit LinkerConfig(const eld::LinkerConfig &Config);
  std::string getTargetCPU() const;
  std::string getArchName() const;
  std::string getTargetVendorName() const;
  std::string getOSName() const;
  std::string getTargetTriple() const;
  uint32_t getMaxGPSize() const;
  bool isAddressSize32Bits() const;
  bool isAddressSize64Bits() const;
  std::string getLinkerCommandline() const;
  std::string getLinkLaunchDirectory() const;
  bool isDynamicLink() const;
  bool hasBSymbolic() const;
  bool hasGCSections() const;
  bool hasUniqueOutputSections() const;

  /// Return options set by -flto-options.
  const std::vector<std::string> &getLTOOptions() const;

  /// Return options set by -flto-options=codegen=
  std::vector<std::string> getLTOCodeGenOptions() const;

  /// return true if the `-flto-options=cache` is specified.
  bool isLTOCacheEnabled() const;

  // Trace options.
  bool shouldTraceLTO() const;

  std::string getMapFileName() const;

private:
  const eld::LinkerConfig &Config;
};

} // namespace eld::plugin

namespace std {
template <> struct hash<eld::plugin::Chunk> {
  std::size_t operator()(const eld::plugin::Chunk &C) const {
    return reinterpret_cast<std::size_t>(C.getFragment());
  }
};

template <> struct hash<eld::plugin::InputFile> {
  std::size_t operator()(const eld::plugin::InputFile &I) const {
    return reinterpret_cast<std::size_t>(I.getInputFile());
  }
};
template <> struct hash<eld::plugin::INIFile> {
  std::size_t operator()(const eld::plugin::INIFile &I) const {
    return reinterpret_cast<std::size_t>(I.getReader());
  }
};

template <> struct hash<eld::plugin::OutputSection> {
  std::size_t operator()(const eld::plugin::OutputSection &O) const {
    return reinterpret_cast<std::size_t>(O.getOutputSection());
  }
};
template <> struct hash<eld::plugin::PluginData> {
  std::size_t operator()(const eld::plugin::PluginData &P) const {
    return reinterpret_cast<std::size_t>(P.getData());
  }
};
template <> struct hash<eld::plugin::LinkerScriptRule> {
  std::size_t operator()(const eld::plugin::LinkerScriptRule &R) const {
    return reinterpret_cast<std::size_t>(R.getRuleContainer());
  }
};
template <> struct hash<eld::plugin::Section> {
  std::size_t operator()(const eld::plugin::Section &S) const {
    return reinterpret_cast<std::size_t>(S.getSection());
  }
};
template <> struct hash<eld::plugin::Segment> {
  std::size_t operator()(const eld::plugin::Segment &S) const {
    return reinterpret_cast<std::size_t>(S.getSegment());
  }
};
template <> struct hash<eld::plugin::Symbol> {
  std::size_t operator()(const eld::plugin::Symbol &S) const {
    return reinterpret_cast<std::size_t>(S.getSymbol());
  }
};
template <> struct hash<eld::plugin::Use> {
  std::size_t operator()(const eld::plugin::Use &U) const {
    return reinterpret_cast<std::size_t>(U.getRelocation());
  }
};
} // namespace std
#endif
