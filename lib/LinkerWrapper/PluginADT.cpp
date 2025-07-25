//===- PluginADT.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "PluginADT.h"
#include "Diagnostics.h"
#include "eld/Fragment/FragUtils.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Input/ArchiveMemberInput.h"
#include "eld/Input/BitcodeFile.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/ObjectFile.h"
#include "eld/Plugin/PluginData.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/INIReader.h"
#include "eld/Support/MemoryArea.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/SymbolResolver/SymbolInfo.h"
#include "eld/Target/Relocator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/GlobPattern.h"
#include "llvm/Support/Timer.h"
#include <memory>
#include <optional>

using namespace eld;
using namespace eld::plugin;

//
//----------------------------------------Chunk---------------------------------
//
std::string Chunk::getName() const {
  if (!m_Fragment)
    return "";
  ELFSection *O = m_Fragment->getOwningSection();
  if (!O)
    return "";
  return O->name().str();
}

plugin::InputFile Chunk::getInputFile() const {
  if (!m_Fragment)
    return plugin::InputFile(nullptr);
  eld::Section *O = m_Fragment->getOwningSection();
  if (!O)
    return plugin::InputFile(nullptr);
  return plugin::InputFile(O->getInputFile());
}

plugin::Section Chunk::getSection() const {
  if (!m_Fragment)
    return plugin::Section(nullptr);
  ELFSection *O = m_Fragment->getOwningSection();
  if (!O)
    return plugin::Section(nullptr);
  return plugin::Section(O);
}

uint32_t Chunk::getSize() const {
  if (!m_Fragment)
    return 0;
  return m_Fragment->size();
}

uint32_t Chunk::getAlignment() const {
  if (!m_Fragment)
    return 0;
  return m_Fragment->alignment();
}

size_t Chunk::getAddress() const {
  if (!m_Fragment)
    return (size_t)-1;
  return m_Fragment->getOutputELFSection()->addr() + m_Fragment->getOffset();
}

bool Chunk::Compare::operator()(const Chunk &A, const Chunk &B) const {
  eld::Fragment *First = A.m_Fragment;
  eld::Fragment *Second = B.m_Fragment;
  if (!First || !Second)
    return false;
  return (First->getOwningSection() != Second->getOwningSection());
}

const char *Chunk::getRawData() const {
  if (!m_Fragment)
    return nullptr;
  if (m_Fragment->getKind() == Fragment::Region ||
      m_Fragment->getKind() == Fragment::RegionFragmentEx)
    return eld::getRegionFromFragment(m_Fragment).data();
  if (m_Fragment->isMergeStr())
    return m_Fragment->getOwningSection()->getContents().data();
  return nullptr;
}

std::vector<plugin::Symbol> Chunk::getSymbols() const {
  std::vector<plugin::Symbol> SymbolsInChunk;
  if (!m_Fragment)
    return SymbolsInChunk;

  eld::ELFSection *Section = m_Fragment->getOwningSection();
  eld::InputFile *I = Section->getInputFile();
  eld::ELFObjectFile *ObjFile = llvm::dyn_cast<eld::ELFObjectFile>(I);

  if (!ObjFile)
    return SymbolsInChunk;

  // Iterate through Local symbols.
  for (auto &L : ObjFile->getLocalSymbols()) {
    eld::ResolveInfo *R = L->resolveInfo();
    // Skip symbols that dont have resolution information and file
    // descriptors.
    if (!R)
      continue;
    // Skip symbols that are not really part of a section.
    // We dont skip COMMON symbols though.
    if (R->isFile() || R->isAbsolute())
      continue;

    eld::Fragment *SymFrag = R->outSymbol()->fragRef()->frag();

    if (m_Fragment != SymFrag)
      continue;

    SymbolsInChunk.push_back(plugin::Symbol(R));
  }

  // Iterate through Global symbols.
  for (auto &S : ObjFile->getSymbols()) {
    eld::ResolveInfo *R = S->resolveInfo();
    // Skip symbols that dont have resolution information and file
    // descriptors.
    if (!R)
      continue;

    // Skip symbols that are not really part of a section.
    // We dont skip COMMON symbols though.
    if (R->isFile() || R->isAbsolute())
      continue;

    // Skip Symbols that are not resolved from this input file.
    if (R->resolvedOrigin() != I)
      continue;

    // Undefined symbols dont belong to any section.
    if (R->isUndef())
      continue;

    // Skip symbols that dont have a FragRef
    if (!R->outSymbol()->hasFragRef())
      continue;

    eld::Fragment *ThisFrag = R->outSymbol()->fragRef()->frag();

    if (!ThisFrag)
      continue;

    eld::ELFSection *ES = ThisFrag->getOwningSection();

    // Skip symbols that are garbage collected.
    if (ES && (ES->isIgnore() || ES->isDiscard()))
      continue;

    if (ThisFrag != m_Fragment)
      continue;

    SymbolsInChunk.push_back(plugin::Symbol(R));
  }
  return SymbolsInChunk;
}

bool Chunk::hasContent() const {
  if (!m_Fragment)
    return false;
  return true;
}

bool Chunk::isProgBits() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->getOwningSection()->isProgBits();
}

bool Chunk::isNoBits() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->getOwningSection()->isNoBits();
}

bool Chunk::isCode() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->getOwningSection()->isCode();
}

bool Chunk::isAlloc() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->getOwningSection()->isAlloc();
}

bool Chunk::isWritable() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->getOwningSection()->isWritable();
}

bool Chunk::isMergeableString() const {
  if (!m_Fragment)
    return false;
  return m_Fragment->isMergeStr();
}

std::vector<plugin::Section> Chunk::getDependentSections() const {
  std::vector<plugin::Section> DependentSections;
  if (!m_Fragment)
    return DependentSections;
  return plugin::Section(m_Fragment->getOwningSection()).getDependentSections();
}

//
//-------------------------------------MergeStringChunk----------------------------------
//

std::vector<plugin::MergeableString>
plugin::MergeStringChunk::getStrings() const {
  std::vector<plugin::MergeableString> Strings;
  for (eld::MergeableString *S :
       llvm::cast<MergeStringFragment>(getFragment())->getStrings())
    Strings.push_back(plugin::MergeableString(S));
  return Strings;
}

//
//---------------------------------plugin::MergeableString---------------------------------
//

plugin::MergeableString::MergeableString(const eld::MergeableString *S)
    : String(S) {}

const char *plugin::MergeableString::getString() const {
  return String->String.data();
}

uint32_t plugin::MergeableString::getSize() const { return String->size(); }

uint32_t plugin::MergeableString::getInputOffset() const {
  return String->InputOffset;
}

uint32_t plugin::MergeableString::getOutputOffset() const {
  return String->OutputOffset;
}

bool plugin::MergeableString::hasOutputOffset() const {
  return String->hasOutputOffset();
}

bool plugin::MergeableString::isMerged() const { return String->Exclude; }

//
//-----------------------------------------INIFile----------------------------------
//

plugin::INIFile::INIFile(const std::string FileName) {
  if (!llvm::sys::fs::exists(FileName)) {
    m_LastError = ErrorCode::FileDoesNotExist;
    return;
  }
  m_LastError = ErrorCode::ReadError;
  m_Reader = new eld::INIReader(FileName);
  auto r = m_Reader->readINIFile();
  if (r && r.value())
    m_LastError = ErrorCode::Success;
}

plugin::INIFile::INIFile(const std::string &FileName, eld::Expected<bool> &de)
    : m_Reader(nullptr), m_LastError(ErrorCode::Success) {
  if (!llvm::sys::fs::exists(FileName)) {
    m_LastError = ErrorCode::FileDoesNotExist;
    de = std::make_unique<DiagnosticEntry>(ErrorDiagnosticEntry(
        Diagnostic::error_file_does_not_exist(), {FileName}));
    return;
  }
  m_LastError = ErrorCode::ReadError;
  m_Reader = new eld::INIReader(FileName);
  auto r = m_Reader->readINIFile();
  if (r && r.value())
    m_LastError = ErrorCode::Success;
  if (!r)
    de = std::move(r);
}

plugin::INIFile::INIFile() {
  m_Reader = new eld::INIReader();
  m_LastError = ErrorCode::Success;
}

// INIFile is a move-only type.
// We cannot use compiler-generated move operations here because
// compiler-generated move-operations copy 'm_Reader' member value
// as there are no built-in move operations for pointers.
INIFile::INIFile(INIFile &&other) noexcept
    : m_Reader(nullptr), m_LastError(ErrorCode::Success) {
  *this = std::move(other);
}

INIFile &INIFile::operator=(INIFile &&other) noexcept {
  if (this == &other)
    return *this;
  std::swap(m_Reader, other.m_Reader);
  std::swap(m_LastError, other.m_LastError);
  return *this;
}

plugin::INIFile::~INIFile() {
  if (m_Reader)
    delete m_Reader;
}

std::string plugin::INIFile::getValue(const std::string Section,
                                      const std::string Item) {
  return m_Reader->containsItem(Section, Item) ? (*m_Reader)[Section][Item]
                                               : "";
}

std::vector<std::pair<std::string, std::string>>
plugin::INIFile::getSection(const std::string Section) const {
  return m_Reader->containsSection(Section)
             ? (*m_Reader)[Section].getItems()
             : std::vector<std::pair<std::string, std::string>>();
}

std::vector<std::string> plugin::INIFile::getSections() const {
  return m_Reader->getSections();
}

void plugin::INIFile::addSection(const std::string &section) {
  if (!m_Reader->containsSection(section))
    (*m_Reader)[section];
}

void plugin::INIFile::insert(const std::string &section, const std::string &K,
                             const std::string &V) {
  (*m_Reader)[section][K] = V;
}

bool plugin::INIFile::containsSection(const std::string &sectionName) const {
  return m_Reader->containsSection(sectionName);
}

bool plugin::INIFile::containsItem(const std::string &sectionName,
                                   const std::string &key) const {
  return m_Reader->containsItem(sectionName, key);
}

plugin::INIFile::operator bool() const { return !m_Reader->empty(); }

std::string plugin::INIFile::getLastErrorAsString() {
  switch (m_LastError) {
  case INIFile::Success:
    return "Success";
    break;
  case INIFile::WriteError:
    return "Error writing file";
    break;
  case INIFile::ReadError:
    return "Error reading file";
    break;
  case INIFile::FileDoesNotExist:
    return "File does not exist";
    break;
  }
}

eld::Expected<INIFile> INIFile::Create(const std::string &filename) {
  eld::Expected<bool> r;
  INIFile I(filename, r);
  if (!r)
    return std::move(r.error());
  return std::move(I);
}

//
//-----------------------------------------Use----------------------------------
//
std::string Use::getName() const {
  if (!m_Relocation)
    return "";
  return m_Relocation->symInfo()->name();
}

plugin::Symbol Use::getSymbol() const {
  if (!m_Relocation)
    return plugin::Symbol(nullptr);
  return plugin::Symbol(m_Relocation->symInfo());
}

Chunk Use::getChunk() const {
  if (!m_Relocation)
    return Chunk(nullptr);
  ResolveInfo *R = m_Relocation->symInfo();
  if (!R->outSymbol())
    return Chunk(nullptr);
  LDSymbol *S = R->outSymbol();
  if (S->hasFragRef())
    return Chunk(S->fragRef()->frag());
  return Chunk(nullptr);
}

Chunk Use::getSourceChunk() const {
  if (!m_Relocation)
    return Chunk(nullptr);
  eld::FragmentRef *Ref = m_Relocation->targetRef();
  if (!Ref)
    return Chunk(nullptr);
  if (Ref->frag())
    return Chunk(Ref->frag());
  return Chunk(nullptr);
}

size_t Use::getSourceAddress() const {
  if (!m_Relocation)
    return UINT64_MAX;
  eld::FragmentRef *Ref = m_Relocation->targetRef();
  if (!Ref)
    return UINT64_MAX;
  if (Ref->frag())
    return Chunk(Ref->frag()).getAddress() + Ref->offset();
  return UINT64_MAX;
}

uint32_t Use::getType() const {
  if (!m_Relocation)
    return UINT32_MAX;
  return m_Relocation->type();
}

Use::Status Use::resetSymbol(plugin::Symbol S) {
  if (!m_Relocation)
    return Use::Status::Error;
  if (!S.getSymbol() || !S.getSymbol()->outSymbol())
    return Use::Status::SymbolDoesNotExist;
  m_Relocation->setSymInfo(S.getSymbol());
  return Use::Status::Ok;
}

off_t Use::getOffsetInChunk() const {
  if (!m_Relocation)
    return -1;
  eld::FragmentRef *Ref = m_Relocation->targetRef();
  if (!Ref)
    return -1;
  return Ref->offset();
}

plugin::Chunk Use::getTargetChunk() const {
  plugin::Chunk NullChunk(nullptr);
  if (!m_Relocation)
    return NullChunk;
  plugin::Symbol S = getSymbol();
  if (!S)
    return NullChunk;
  return S.getChunk();
}

off_t Use::getTargetChunkOffset() const {
  if (!m_Relocation)
    return -1;
  return m_Relocation->addend();
}

//
//--------------------------------------Section---------------------------------
//
std::string plugin::Section::getName() const {
  if (!m_Section)
    return "";
  return m_Section->name().str();
}

plugin::InputFile plugin::Section::getInputFile() const {
  if (!m_Section)
    return plugin::InputFile(nullptr);
  return plugin::InputFile(m_Section->getInputFile());
}

uint32_t plugin::Section::getSize() const {
  if (!m_Section)
    return 0;
  return m_Section->size();
}

uint32_t plugin::Section::getIndex() const {
  if (!m_Section)
    return -1;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return -1;
  return S->getIndex();
}

uint32_t plugin::Section::getAlignment() const {
  if (!m_Section)
    return 0;
  return m_Section->getAddrAlign();
}

bool plugin::Section::isProgBits() const {
  if (!m_Section)
    return false;
  if (auto *S = llvm::dyn_cast<ELFSection>(m_Section))
    return S->isProgBits();
  return false;
}

bool plugin::Section::isNoBits() const {
  if (!m_Section)
    return false;
  if (auto *S = llvm::dyn_cast<ELFSection>(m_Section))
    return S->isNoBits();
  return false;
}

bool plugin::Section::isAlloc() const {
  if (!m_Section)
    return false;
  if (auto *S = llvm::dyn_cast<ELFSection>(m_Section))
    return S->isAlloc();
  return false;
}

bool plugin::Section::isWritable() const {
  if (!m_Section)
    return false;
  if (auto *S = llvm::dyn_cast<ELFSection>(m_Section))
    return S->isWritable();
  return false;
}

bool plugin::Section::matchPattern(const std::string &Pattern) const {
  if (!m_Section)
    return false;
  auto E = llvm::GlobPattern::create(Pattern);
  if (!E) {
    (void)(E.takeError());
    return false;
  }
  return E->match(getName());
}

void plugin::Section::markAsDiscarded() {
  if (!m_Section)
    return;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return;
  S->setKind(LDFileFormat::Discard);
}

bool plugin::Section::isDiscarded() const {
  if (!m_Section)
    return false;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return false;
  return (S->isDiscard());
}

bool plugin::Section::isGarbageCollected() const {
  if (!m_Section)
    return false;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return false;
  return (S->isIgnore());
}

std::vector<plugin::Section> plugin::Section::getDependentSections() const {
  std::vector<plugin::Section> DependentSections;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return DependentSections;
  const llvm::SmallVectorImpl<ELFSection *> &D = S->getDependentSections();
  if (!D.size())
    return DependentSections;
  for (auto &Sec : D)
    DependentSections.push_back(plugin::Section(Sec));
  return DependentSections;
}

std::vector<plugin::Symbol> plugin::Section::getSymbols() const {
  std::vector<plugin::Symbol> SymbolsInSection;
  if (!m_Section)
    return SymbolsInSection;
  eld::InputFile *I = m_Section->getInputFile();
  eld::ELFObjectFile *ObjFile = llvm::dyn_cast<eld::ELFObjectFile>(I);

  if (!ObjFile)
    return SymbolsInSection;

  // Iterate through Local symbols.
  for (auto &L : ObjFile->getLocalSymbols()) {
    eld::ResolveInfo *R = L->resolveInfo();
    // Skip symbols that dont have resolution information and file
    // descriptors.
    if (!R)
      continue;
    // Skip symbols that are not really part of a section.
    // We dont skip COMMON symbols though.
    if (R->isFile() || R->isAbsolute())
      continue;

    eld::ELFSection *ES = R->getOwningSection();

    if (m_Section != ES)
      continue;

    SymbolsInSection.push_back(plugin::Symbol(R));
  }

  // Iterate through Global symbols.
  for (auto &S : ObjFile->getSymbols()) {
    eld::ResolveInfo *R = S->resolveInfo();
    // Skip symbols that dont have resolution information and file
    // descriptors.
    if (!R)
      continue;

    // Skip symbols that are not really part of a section.
    // We dont skip COMMON symbols though.
    if (R->isFile() || R->isAbsolute())
      continue;

    // Skip Symbols that are not resolved from this input file.
    if (R->resolvedOrigin() != I)
      continue;

    // Undefined symbols dont belong to any section.
    if (R->isUndef()) {
      continue;
    }

    // If symbol is common, commons dont belong to any
    // section.
    if (R->isCommon())
      continue;

    // Skip symbols that dont have a FragRef
    if (!R->outSymbol()->hasFragRefSection())
      continue;

    eld::ELFSection *ES = R->getOwningSection();

    if (ES != m_Section)
      continue;

    // Skip symbols that are garbage collected.
    if (ES && (ES->isIgnore() || ES->isDiscard()))
      continue;

    SymbolsInSection.push_back(plugin::Symbol(R));
  }
  return SymbolsInSection;
}

void plugin::Section::setLinkerScriptRule(plugin::LinkerScriptRule R) {
  if (!m_Section)
    return;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  RuleContainer *RC = R.getRuleContainer();
  S->setOutputSection(RC->getSection()->getOutputSection());
  S->setMatchedLinkerScriptRule(RC);
  RC->incMatchCount();
}

uint64_t plugin::Section::getSectionHash() const {
  ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(m_Section);
  if (!ELFSect)
    return 0;
  return ELFSect->getSectionHash();
}

bool plugin::Section::hasOldInputFile() const {
  if (!m_Section)
    return false;
  ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(m_Section);
  if (!ELFSect)
    return false;
  return (ELFSect->hasOldInputFile());
}

bool plugin::Section::isELFSection() const {
  ELFSection *ELFSect = llvm::dyn_cast<ELFSection>(m_Section);
  return (ELFSect != nullptr);
}

bool plugin::Section::isIgnore() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isIgnore();
  return false;
}

bool plugin::Section::isNull() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isNullKind();
  return false;
}

bool plugin::Section::isStackNote() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isNoteGNUStack();
  return false;
}

bool plugin::Section::isNamePool() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isNamePool();
  return false;
}

bool plugin::Section::isRelocation() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isRelocationKind();
  return false;
}

bool plugin::Section::isGroup() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->isGroupKind();
  return false;
}

std::vector<plugin::Chunk> plugin::Section::getChunks() const {
  std::vector<plugin::Chunk> Chunks;
  if (!m_Section)
    return Chunks;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return Chunks;
  for (auto &F : S->getFragmentList())
    Chunks.push_back(plugin::Chunk(F));
  return Chunks;
}

bool plugin::Section::isCode() const {
  if (!m_Section)
    return false;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return false;
  return S->isCode();
}

bool plugin::Section::isNote() const {
  if (!m_Section)
    return false;
  ELFSection *S = llvm::dyn_cast<ELFSection>(m_Section);
  if (!S)
    return false;
  return S->isNote();
}

LinkerScriptRule plugin::Section::getLinkerScriptRule() const {
  LinkerScriptRule R;
  if (!m_Section)
    return R;
  return plugin::LinkerScriptRule(m_Section->getMatchedLinkerScriptRule());
}

plugin::OutputSection plugin::Section::getOutputSection() const {
  if (!m_Section)
    return plugin::OutputSection(nullptr);
  OutputSectionEntry *S = m_Section->getOutputSection();
  if (!S)
    return plugin::OutputSection(nullptr);
  return plugin::OutputSection(S);
}

std::optional<uint64_t> plugin::Section::getOffset() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->offset();
  return std::nullopt;
}

std::optional<uint32_t> plugin::Section::getEntrySize() const {
  if (ELFSection *S = llvm::dyn_cast_or_null<ELFSection>(m_Section))
    return S->getEntSize();
  return std::nullopt;
}

const size_t plugin::Section::SHF_WRITE = llvm::ELF::SHF_WRITE;
const size_t plugin::Section::SHF_ALLOC = llvm::ELF::SHF_ALLOC;
const size_t plugin::Section::SHF_EXECINSTR = llvm::ELF::SHF_EXECINSTR;
const size_t plugin::Section::SHF_MERGE = llvm::ELF::SHF_MERGE;
const size_t plugin::Section::SHF_STRINGS = llvm::ELF::SHF_STRINGS;
const size_t plugin::Section::SHF_GNU_RETAIN = llvm::ELF::SHF_GNU_RETAIN;

const size_t plugin::Section::SHT_NULL = llvm::ELF::SHT_NULL;
const size_t plugin::Section::SHT_PROGBITS = llvm::ELF::SHT_PROGBITS;
const size_t plugin::Section::SHT_NOTE = llvm::ELF::SHT_NOTE;
const size_t plugin::Section::SHT_NOBITS = llvm::ELF::SHT_NOBITS;

//
//------------------------------------Segment-----------------------------------
//
std::string Segment::getName() const { return S->name(); }

uint32_t Segment::getType() const { return S->type(); }

uint64_t Segment::getOffset() const { return S->offset(); }

uint64_t Segment::getPhysicalAddress() const { return S->paddr(); }

uint64_t Segment::getVirtualAddress() const { return S->vaddr(); }

uint64_t Segment::getFileSize() const { return S->filesz(); }

uint64_t Segment::getMemorySize() const { return S->memsz(); }

uint32_t Segment::getSegmentFlags() const { return S->flag(); }

uint64_t Segment::getPageAlignment() const { return S->align(); }

bool Segment::isLoadSegment() const { return S->isLoadSegment(); }

bool Segment::isTLSSegment() const { return getType() == llvm::ELF::PT_TLS; }

bool Segment::isDynamicSegment() const {
  return getType() == llvm::ELF::PT_DYNAMIC;
}

bool Segment::isRelROSegment() const {
  return getType() == llvm::ELF::PT_GNU_RELRO;
}

bool Segment::isNoteSegment() const { return getType() == llvm::ELF::PT_NOTE; }

bool Segment::isNullSegment() const { return getType() == llvm::ELF::PT_NULL; }

uint32_t Segment::getMaxSectionAlign() const { return S->getMaxSectionAlign(); }

eld::Expected<std::vector<OutputSection>>
plugin::Segment::getOutputSections(const LinkerWrapper &LW) const {
  if (LW.getState() < LinkerWrapper::CreatingSections)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 __FUNCTION__,
                                 "'CreatingSections, AfterLayout'"});
  std::vector<OutputSection> Sections;
  for (eld::OutputSectionEntry *O : S->sections())
    Sections.push_back(OutputSection(O));
  return Sections;
}

//
//-------------------------------OutputSection---------------------------------
//
std::string OutputSection::getName() const {
  if (!m_OutputSection)
    return "";
  return m_OutputSection->getSection()->name().str();
}

uint64_t OutputSection::getAlignment() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->getAddrAlign();
}

uint64_t OutputSection::getFlags() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->getFlags();
}

uint64_t OutputSection::getType() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->getType();
}

uint64_t OutputSection::getHash() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getHash();
}

std::vector<eld::plugin::LinkerScriptRule> OutputSection::getRules() {
  std::vector<eld::plugin::LinkerScriptRule> rules;
  for (RuleContainer *RC : *m_OutputSection) {
    rules.push_back(eld::plugin::LinkerScriptRule(RC));
  }
  return rules;
}

uint64_t OutputSection::getIndex() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->getIndex();
}

std::vector<plugin::LinkerScriptRule> OutputSection::getLinkerScriptRules() {
  std::vector<plugin::LinkerScriptRule> Rules;
  if (!m_OutputSection)
    return Rules;
  for (const auto &In : m_OutputSection->getRuleContainer())
    Rules.push_back(plugin::LinkerScriptRule(In));
  return Rules;
}

uint64_t OutputSection::getSize() const {
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->size();
}

eld::Expected<uint64_t>
OutputSection::getVirtualAddress(const LinkerWrapper &LW) const {
  if (LW.getState() < LinkerWrapper::State::AfterLayout)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 __FUNCTION__,
                                 "'CreatingSegments, AfterLayout'"});
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->addr();
}

eld::Expected<uint64_t>
OutputSection::getPhysicalAddress(const LinkerWrapper &LW) const {
  if (LW.getState() != LinkerWrapper::State::AfterLayout)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 __FUNCTION__, "'AfterLayout'"});
  if (!m_OutputSection)
    return 0;
  return m_OutputSection->getSection()->pAddr();
}

eld::Expected<std::vector<Segment>>
OutputSection::getSegments(const LinkerWrapper &LW) const {
  auto ExpSegments = LW.getSegmentsForOutputSection(*this);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpSegments);
  return ExpSegments.value();
}

eld::Expected<std::optional<Segment>>
OutputSection::getLoadSegment(const LinkerWrapper &LW) const {
  if (LW.getState() < LinkerWrapper::CreatingSections)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 __FUNCTION__,
                                 "'CreatingSections, AfterLayout'"});
  if (!m_OutputSection)
    return std::nullopt;
  if (auto *Segment = m_OutputSection->getLoadSegment())
    return plugin::Segment(Segment);
  return std::nullopt;
}

eld::Expected<uint64_t> OutputSection::getOffset() const {
  if (!getOutputSection()->getSection()->hasOffset())
    return std::make_unique<DiagnosticEntry>(
        Diag::error_offset_not_assigned_for_output_section,
        std::vector<std::string>{getName()});
  return getOutputSection()->getSection()->offset();
}

eld::Expected<void> OutputSection::setOffset(uint64_t Offset,
                                             const LinkerWrapper &LW) {
  if (LW.getState() < LinkerWrapper::AfterLayout)
    return std::make_unique<DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 LLVM_PRETTY_FUNCTION,
                                 "'CreatingSegments, AfterLayout'"});
  getOutputSection()->getSection()->setOffset(Offset);
  return {};
}

bool OutputSection::isNoBits() const {
  return getOutputSection()->getSection()->isNoBits();
}

bool OutputSection::isEmpty() const { return m_OutputSection->empty(); }

bool OutputSection::isDiscard() const { return m_OutputSection->isDiscard(); }

bool OutputSection::isCode() const {
  return getOutputSection()->getSection()->isCode();
}

bool OutputSection::isAlloc() const {
  return getOutputSection()->getSection()->isAlloc();
}

std::vector<plugin::Stub> OutputSection::getStubs() const {
  if (!m_OutputSection)
    return {};
  std::vector<plugin::Stub> trampolines;
  for (auto iter = m_OutputSection->islandsBegin();
       iter != m_OutputSection->islandsEnd(); ++iter) {
    trampolines.emplace_back(*iter);
  }
  return trampolines;
}

Expected<void> OutputSection::setPluginOverride(Plugin *P,
                                                const LinkerWrapper &LW) {
  if (LW.getState() != LinkerWrapper::State::CreatingSegments)
    return std::make_unique<plugin::DiagnosticEntry>(
        Diag::error_invalid_link_state,
        std::vector<std::string>{std::string(LW.getCurrentLinkStateAsStr()),
                                 __FUNCTION__, "'CreatingSegments'"});
  getOutputSection()->prolog().setPlugin(
      make<PluginCmd>(P->getType(), P->GetName(), "", ""));
  return {};
}

//
//--------------------------------------Symbol---------------------------------
//
std::string plugin::Symbol::getName() const {
  if (!m_Symbol)
    return "";
  return m_Symbol->name();
}

Chunk plugin::Symbol::getChunk() const {
  if (!m_Symbol)
    return Chunk(nullptr);
  eld::LDSymbol *Sym = m_Symbol->outSymbol();
  if (!Sym)
    return Chunk(nullptr);
  if (!Sym->hasFragRefSection())
    return Chunk(nullptr);
  return Chunk(Sym->fragRef()->frag());
}

bool plugin::Symbol::isLocal() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isLocal());
}

bool plugin::Symbol::isWeak() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isWeak());
}

bool plugin::Symbol::isGlobal() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isGlobal());
}

bool plugin::Symbol::isFunction() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isFunc());
}

bool plugin::Symbol::isObject() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isObject());
}

bool plugin::Symbol::isFile() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isFile());
}

bool plugin::Symbol::isNoType() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isNoType());
}

bool plugin::Symbol::isGarbageCollected() const {
  if (!m_Symbol)
    return false;
  if (!m_Symbol->outSymbol())
    return false;
  // Common symbols ignore property is set to true if
  // they are garbage collected.
  if (isCommon() && m_Symbol->outSymbol()->shouldIgnore())
    return true;
  // If they dont have a section, it could be an undefined
  // symbol.
  if (!m_Symbol->outSymbol()->hasFragRefSection())
    return false;
  eld::ELFSection *ES = m_Symbol->getOwningSection();
  if (ES && (ES->isIgnore() || ES->isDiscard()))
    return true;
  return false;
}

bool plugin::Symbol::isUndef() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isUndef());
}

bool plugin::Symbol::isCommon() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isCommon());
}

bool plugin::Symbol::isSection() const {
  if (!m_Symbol)
    return false;
  return (m_Symbol->isSection());
}

std::string plugin::Symbol::getResolvedPath() const {
  if (!m_Symbol)
    return "";
  return (m_Symbol->resolvedOrigin()->getInput()->decoratedPath());
}

uint32_t plugin::Symbol::getSize() const {
  if (!m_Symbol)
    return 0;
  return m_Symbol->size();
}

size_t plugin::Symbol::getValue() const {
  if (!m_Symbol)
    return (size_t)-1;
  if (!m_Symbol->outSymbol())
    return -1;
  return m_Symbol->outSymbol()->value();
}

size_t plugin::Symbol::getAddress() const {
  if (!m_Symbol)
    return UINT64_MAX;
  eld::LDSymbol *Sym = m_Symbol->outSymbol();
  if (!Sym)
    return UINT64_MAX;
  if (Sym->resolveInfo() && Sym->resolveInfo()->isAbsolute())
    return Sym->value();
  if (!Sym->hasFragRefSection())
    return UINT64_MAX;
  return Chunk(Sym->fragRef()->frag()).getAddress() + Sym->fragRef()->offset();
}

off_t plugin::Symbol::getOffsetInChunk() const {
  if (!m_Symbol)
    return -1;
  eld::LDSymbol *Sym = m_Symbol->outSymbol();
  if (!Sym)
    return -1;
  if (!Sym->hasFragRefSection())
    return -1;
  return Sym->fragRef()->offset();
}

uint64_t plugin::Symbol::getSymbolIndex() const {
  if (!m_Symbol || !m_Symbol->outSymbol())
    return 0;
  return m_Symbol->outSymbol()->getSymbolIndex();
}

plugin::InputFile plugin::Symbol::getInputFile() {
  if (!m_Symbol)
    return InputFile(nullptr);
  return InputFile(m_Symbol->resolvedOrigin());
}

//
//-----------------------------LinkerScriptRule---------------------------------
//

plugin::Script::InputSectionSpec LinkerScriptRule::getInputSectionSpec() const {
  return plugin::Script::InputSectionSpec(
      const_cast<InputSectDesc *>(m_RuleContainer->desc()));
}

plugin::OutputSection LinkerScriptRule::getOutputSection() const {
  if (!m_RuleContainer)
    return plugin::OutputSection(nullptr);
  auto S = m_RuleContainer->getSection();
  ASSERT(S, "Must not be null");
  return plugin::OutputSection(S->getOutputSection());
}

std::optional<uint64_t> LinkerScriptRule::getHash() const {
  return m_RuleContainer->getRuleHash();
}

bool LinkerScriptRule::canMoveSections() const {
  if (!m_RuleContainer)
    return false;
  return !m_RuleContainer->isFixed();
}

bool LinkerScriptRule::isLinkerInsertedRule() const {
  if (!m_RuleContainer)
    return false;
  return m_RuleContainer->isSpecial();
}

bool LinkerScriptRule::isKeep() const {
  if (!m_RuleContainer)
    return false;
  return m_RuleContainer->isEntry();
}

bool LinkerScriptRule::hasExpressions() const {
  if (!m_RuleContainer)
    return false;
  return m_RuleContainer->symAssignments().size();
}

std::vector<plugin::Section> LinkerScriptRule::getSections() {
  std::vector<plugin::Section> S;
  for (auto &ELFSect : m_RuleContainer->getMatchedInputSections()) {
    if (ELFSect && (ELFSect->isDiscard() || ELFSect->isIgnore()))
      continue;
    S.push_back(plugin::Section(ELFSect));
  }
  return S;
}

std::vector<plugin::Section> LinkerScriptRule::getMatchedSections() const {
  std::vector<plugin::Section> S;
  for (auto &ELFSect : m_RuleContainer->getMatchedInputSections()) {
    S.push_back(plugin::Section(ELFSect));
  }
  return S;
}

std::string LinkerScriptRule::asString() const {
  if (!m_RuleContainer)
    return "";
  return m_RuleContainer->getAsString();
}

bool LinkerScriptRule::doesExpressionModifyDot() const {
  if (!hasExpressions())
    return false;
  for (auto &A : m_RuleContainer->symAssignments()) {
    if (A->isDot())
      return true;
  }
  return false;
}

std::vector<plugin::Chunk> LinkerScriptRule::getChunks() const {
  std::vector<plugin::Chunk> CVect;
  auto &Fragments = m_RuleContainer->getSection()->getFragmentList();
  for (auto &F : Fragments)
    CVect.push_back(plugin::Chunk(F));
  return CVect;
}

void LinkerScriptRule::updateChunks(std::vector<plugin::Chunk> C) {
  m_RuleContainer->clearSections();
  m_RuleContainer->clearFragments();
  for (auto &Chunk : C) {
    Chunk.getFragment()->getOwningSection()->setOutputSection(
        m_RuleContainer->getSection()->getOutputSection());
    Chunk.getFragment()->getOwningSection()->setMatchedLinkerScriptRule(
        m_RuleContainer);
    m_RuleContainer->getSection()->addFragment(Chunk.getFragment());
  }
  m_RuleContainer->setDirty();
}

LinkerScriptRule::State
LinkerScriptRule::updateChunks(std::vector<plugin::Chunk> C, bool Verify) {
  if (!Verify) {
    updateChunks(C);
    return LinkerScriptRule::State::Ok;
  }
  auto &List = m_RuleContainer->getSection()->getFragmentList();
  if (!List.size())
    return LinkerScriptRule::State::NotEmpty;
  m_RuleContainer->clearSections();
  m_RuleContainer->clearFragments();
  for (auto &Chunk : C) {
    auto &L = m_RuleContainer->getSection()->getFragmentList();
    if (std::find(L.begin(), L.end(), Chunk.getFragment()) != L.end())
      return LinkerScriptRule::State::DuplicateChunk;
    m_RuleContainer->getSection()->addFragment(Chunk.getFragment());
    Chunk.getFragment()->getOwningSection()->setOutputSection(
        m_RuleContainer->getSection()->getOutputSection());
    Chunk.getFragment()->getOwningSection()->setMatchedLinkerScriptRule(
        m_RuleContainer);
  }
  m_RuleContainer->setDirty();
  return LinkerScriptRule::State::Ok;
}

void LinkerScriptRule::addChunk(plugin::Chunk C) {
  m_RuleContainer->getSection()->addFragment(C.getFragment());
  C.getFragment()->getOwningSection()->setOutputSection(
      m_RuleContainer->getSection()->getOutputSection());
  C.getFragment()->getOwningSection()->setMatchedLinkerScriptRule(
      m_RuleContainer);
  m_RuleContainer->setDirty();
}

LinkerScriptRule::State LinkerScriptRule::addChunk(plugin::Chunk C,
                                                   bool Verify) {
  if (!Verify) {
    addChunk(C);
    return LinkerScriptRule::State::Ok;
  }
  auto &L = m_RuleContainer->getSection()->getFragmentList();
  if (std::find(L.begin(), L.end(), C.getFragment()) != L.end())
    return LinkerScriptRule::State::DuplicateChunk;
  m_RuleContainer->getSection()->addFragment(C.getFragment());
  C.getFragment()->getOwningSection()->setOutputSection(
      m_RuleContainer->getSection()->getOutputSection());
  C.getFragment()->getOwningSection()->setMatchedLinkerScriptRule(
      m_RuleContainer);
  m_RuleContainer->setDirty();
  return LinkerScriptRule::State::Ok;
}

void LinkerScriptRule::removeChunk(plugin::Chunk C) {
  m_RuleContainer->getSection()->removeFragment(C.getFragment());
  m_RuleContainer->setDirty();
}

LinkerScriptRule::State LinkerScriptRule::removeChunk(plugin::Chunk C,
                                                      bool Verify) {
  if (!Verify) {
    removeChunk(C);
    return LinkerScriptRule::State::Ok;
  }
  auto &L = m_RuleContainer->getSection()->getFragmentList();
  if (!L.size())
    return LinkerScriptRule::State::Empty;
  if (std::find(L.begin(), L.end(), C.getFragment()) == L.end())
    return LinkerScriptRule::State::NoChunk;
  removeChunk(C);
  return LinkerScriptRule::State::Ok;
}

std::vector<plugin::Chunk> LinkerScriptRule::getChunks(plugin::Section S) {
  std::vector<plugin::Chunk> CVect;
  auto &Fragments = m_RuleContainer->getSection()->getFragmentList();
  for (auto &F : Fragments) {
    if (F->getOwningSection() == S.getSection())
      CVect.push_back(plugin::Chunk(F));
  }
  return CVect;
}

//
//-----------------------------InputFile---------------------------------
//
std::string plugin::InputFile::getFileName() const {
  if (!m_InputFile)
    return "";
  return m_InputFile->getInput()->getResolvedPath().native();
}

bool plugin::InputFile::hasInputFile() const {
  return (m_InputFile != nullptr);
}

bool plugin::InputFile::isArchive() const {
  if (!m_InputFile)
    return false;
  return m_InputFile->getInput()->isArchiveMember();
}

bool plugin::InputFile::isBitcode() const {
  if (!m_InputFile)
    return false;
  return m_InputFile->isBitcode();
}

std::string plugin::InputFile::getMemberName() const {
  if (!isArchive())
    return "";
  return m_InputFile->getInput()->getName();
}

std::vector<plugin::Symbol> plugin::InputFile::getSymbols() const {
  std::vector<plugin::Symbol> Symbols;
  if (!m_InputFile)
    return Symbols;
  eld::ELFObjectFile *ObjFile = llvm::dyn_cast<eld::ELFObjectFile>(m_InputFile);
  if (!ObjFile)
    return Symbols;
  // Iterate through Local symbols.
  for (auto &L : ObjFile->getLocalSymbols()) {
    eld::ResolveInfo *R = (*L).resolveInfo();
    // Skip symbols that dont have resolution information and file
    // descriptors.
    if (!R)
      continue;
    Symbols.push_back(plugin::Symbol(R));
  }
  for (auto &S : ObjFile->getSymbols()) {
    eld::ResolveInfo *R = S->resolveInfo();

    if (!R || !R->resolvedOrigin())
      continue;

    // Skip Symbols that are not resolved from this input file.
    if (R->resolvedOrigin() != m_InputFile)
      continue;

    if (R->isUndef()) {
      Symbols.push_back(plugin::Symbol(R));
      continue;
    }

    if (R->isCommon()) {
      Symbols.push_back(plugin::Symbol(R));
      continue;
    }

    // Skip symbols that dont have a FragRef
    if (!R->outSymbol()->hasFragRefSection())
      continue;

    Symbols.push_back(plugin::Symbol(R));
  }
  return Symbols;
}

std::vector<plugin::Section> plugin::InputFile::getSections() const {
  std::vector<plugin::Section> Sections;
  if (!m_InputFile)
    return Sections;
  eld::ObjectFile *ObjFile = llvm::dyn_cast<eld::ObjectFile>(m_InputFile);
  if (!ObjFile)
    return Sections;
  for (auto &S : ObjFile->getSections())
    Sections.push_back(plugin::Section(S));
  return Sections;
}

std::optional<plugin::Section> plugin::InputFile::getSection(uint64_t i) const {
  if (!m_InputFile)
    return std::nullopt;
  eld::ObjectFile *ObjFile = llvm::dyn_cast<eld::ObjectFile>(m_InputFile);
  if (!ObjFile)
    return std::nullopt;
  if (i >= ObjFile->getNumSections())
    return std::nullopt;
  return plugin::Section(ObjFile->getSection(i));
}

const char *plugin::InputFile::getMemoryBuffer() const {
  if (!m_InputFile)
    return nullptr;
  return m_InputFile->getContents().data();
}

std::string_view plugin::InputFile::slice(uint32_t offset,
                                          uint32_t size) const {
  if (!m_InputFile)
    return {};
  llvm::StringRef contents = m_InputFile->getSlice(offset, size);
  return std::string_view{contents.data(), contents.size()};
}

size_t plugin::InputFile::getSize() const {
  if (!m_InputFile)
    return 0;
  return m_InputFile->getSize();
}

uint32_t plugin::InputFile::getOrdinal() const {
  return m_InputFile->getInput()->getInputOrdinal();
}

bool plugin::InputFile::isInternal() const {
  if (!m_InputFile)
    return false;
  return m_InputFile->isInternal();
}

bool plugin::InputFile::isArchiveMember() const {
  if (!m_InputFile)
    return false;
  return m_InputFile->getInput()->isArchiveMember();
}

std::optional<plugin::InputFile> plugin::InputFile::getArchiveFile() const {
  if (!m_InputFile)
    return std::nullopt;
  eld::ArchiveMemberInput *AMI =
      llvm::dyn_cast<eld::ArchiveMemberInput>(m_InputFile->getInput());
  if (!AMI)
    return std::nullopt;
  return plugin::InputFile(AMI->getArchiveFile());
}

std::string plugin::InputFile::decoratedPath() const {
  return m_InputFile->getInput()->decoratedPath();
}

std::string plugin::InputFile::getRealPath() const {
  return m_InputFile->getInput()->getResolvedPath().getFullPath();
}

//
//-----------------------------BitcodeFile---------------------------------
//
plugin::BitcodeFile::BitcodeFile(eld::BitcodeFile &F) : plugin::InputFile(&F) {}

eld::BitcodeFile &plugin::BitcodeFile::getBitcodeFile() const {
  return *llvm::dyn_cast<eld::BitcodeFile>(InputFile::getInputFile());
}

llvm::lto::InputFile &plugin::BitcodeFile::getInputFile() const {
  return getBitcodeFile().getInputFile();
}

bool plugin::BitcodeFile::findIfKeptComdat(unsigned index) const {
  return getBitcodeFile().findIfKeptComdat(index);
}

//
//-----------------------------PluginData---------------------------------
//
uint32_t plugin::PluginData::getKey() const { return m_Data->getKey(); }

void *plugin::PluginData::getData() const { return m_Data->getData(); }

std::string plugin::PluginData::getAnnotation() const {
  return m_Data->getAnnotation();
}

//
//-----------------------------AutoTimer-------------------------------
//
plugin::AutoTimer::AutoTimer(llvm::Timer *T) : m_Timer(T) { start(); }

void plugin::AutoTimer::start() {
  if (m_Timer)
    m_Timer->startTimer();
}

void plugin::AutoTimer::end() {
  if (m_Timer)
    m_Timer->stopTimer();
}

plugin::AutoTimer::~AutoTimer() { end(); }

//
//-----------------------------Timer---------------------------------
//
plugin::Timer::Timer(llvm::Timer *T) : m_Timer(T) {}

void plugin::Timer::start() {
  if (m_Timer)
    m_Timer->startTimer();
}

void plugin::Timer::end() {
  if (m_Timer)
    m_Timer->stopTimer();
}

plugin::Timer::~Timer() {}

//
//-----------------------------RelocationHandler-----------------------
//
plugin::RelocationHandler::RelocationHandler(eld::Relocator *R)
    : Relocator(R) {}

uint32_t plugin::RelocationHandler::getRelocationType(std::string Name) const {
  return Relocator->getRelocType(Name);
}

std::string plugin::RelocationHandler::getRelocationName(uint32_t Type) const {
  return Relocator->getName(Type);
}

plugin::RelocationHandler::~RelocationHandler() {}

//
//-----------------------------MemoryBuffer----------------------------
//

plugin::MemoryBuffer::MemoryBuffer(std::unique_ptr<eld::MemoryArea> Buf) {
  m_Buffer = std::move(Buf);
}

plugin::MemoryBuffer::MemoryBuffer(plugin::MemoryBuffer &&other) noexcept
    : m_Buffer(nullptr) {
  *this = std::move(other);
}

plugin::MemoryBuffer &
plugin::MemoryBuffer::operator=(plugin::MemoryBuffer &&other) noexcept {
  if (this == &other)
    return *this;
  std::swap(m_Buffer, other.m_Buffer);
  return *this;
}

eld::Expected<MemoryBuffer>
plugin::MemoryBuffer::getBuffer(const std::string &Name, const uint8_t *Data,
                                size_t Length, bool isNullTerminated) {
  if (Length == 0) {
    eld::Expected<bool> mb = std::make_unique<DiagnosticEntry>(
        ErrorDiagnosticEntry(eld::Diag::error_empty_data, {Name}));
    return std::move(mb.error());
  }
  std::unique_ptr<eld::MemoryArea> MA =
      eld::MemoryArea::CreateUniqueRef(Name, Data, Length, isNullTerminated);
  return MemoryBuffer(std::move(MA));
}

std::string plugin::MemoryBuffer::getName() const {
  return m_Buffer->getName().str();
}

const uint8_t *plugin::MemoryBuffer::getContent() const {
  return reinterpret_cast<const uint8_t *>(m_Buffer->getContents().data());
}

size_t plugin::MemoryBuffer::getSize() const { return m_Buffer->size(); }

std::unique_ptr<eld::MemoryArea> plugin::MemoryBuffer::getBuffer() {
  return std::move(m_Buffer);
}

plugin::MemoryBuffer::~MemoryBuffer() {}

InputSymbol::InputSymbol() : Sym(nullptr), SymInfo(nullptr) {}

InputSymbol::InputSymbol(eld::LDSymbol *sym, std::string_view symName,
                         std::unique_ptr<eld::SymbolInfo> symInfo)
    : Sym(sym), SymName(symName), SymInfo(std::move(symInfo)) {}

InputSymbol::InputSymbol(const InputSymbol &rhs) { *this = rhs; }

InputSymbol &InputSymbol::operator=(const InputSymbol &rhs) {
  if (this == &rhs)
    return *this;
  Sym = rhs.Sym;
  SymName = rhs.SymName;
  SymInfo = (rhs.SymInfo ? std::make_unique<SymbolInfo>(*rhs.SymInfo)
                         : std::unique_ptr<SymbolInfo>(nullptr));
  return *this;
}

InputSymbol::~InputSymbol() {}

std::string_view InputSymbol::getName() const { return SymName; }

bool InputSymbol::isLocal() const {
  if (!SymInfo)
    return false;
  return SymInfo->getSymbolBinding() == SymbolInfo::SymbolBinding::Local;
}

plugin::LinkerConfig::LinkerConfig(const eld::LinkerConfig &Config)
    : Config(Config) {}

std::string plugin::LinkerConfig::getTargetCPU() const {
  return Config.targets().getTargetCPU();
}

std::string plugin::LinkerConfig::getArchName() const {
  return Config.targets().triple().getArchName().str();
}

std::string plugin::LinkerConfig::getTargetVendorName() const {
  return Config.targets().triple().getVendorName().str();
}

std::string plugin::LinkerConfig::getOSName() const {
  return Config.targets().triple().getOSName().str();
}

std::string plugin::LinkerConfig::getTargetTriple() const {
  const llvm::Triple &TheTriple = Config.targets().triple();
  return TheTriple.str();
}

uint32_t plugin::LinkerConfig::getMaxGPSize() const {
  return Config.options().getGPSize();
}

bool plugin::LinkerConfig::isAddressSize32Bits() const {
  return Config.targets().is32Bits();
}

bool plugin::LinkerConfig::isAddressSize64Bits() const {
  return Config.targets().is64Bits();
}

std::string plugin::LinkerConfig::getLinkerCommandline() const {
  std::string CommandLine;
  std::string Separator = "";
  for (auto arg : Config.options().args()) {
    CommandLine.append(Separator);
    Separator = " ";
    if (arg)
      CommandLine.append(std::string(arg));
  }
  return CommandLine;
}

std::string plugin::LinkerConfig::getLinkLaunchDirectory() const {
  return Config.options().getLinkLaunchDirectory();
}

bool plugin::LinkerConfig::isDynamicLink() const {
  return Config.isCodeDynamic();
}

bool plugin::LinkerConfig::hasBSymbolic() const {
  return Config.options().bsymbolic();
}

bool plugin::LinkerConfig::hasGCSections() const {
  return Config.options().gcSections();
}

bool plugin::LinkerConfig::hasUniqueOutputSections() const {
  return Config.options().shouldEmitUniqueOutputSections();
}

const std::vector<std::string> &plugin::LinkerConfig::getLTOOptions() const {
  return Config.options().getUnparsedLTOOptions();
}

std::vector<std::string> plugin::LinkerConfig::getLTOCodeGenOptions() const {
  std::vector<std::string> v;
  for (const auto &i : Config.options().codeGenOpts())
    v.emplace_back(i);
  return v;
}

bool plugin::LinkerConfig::isLTOCacheEnabled() const {
  return Config.options().isLTOCacheEnabled();
}

bool plugin::LinkerConfig::shouldTraceLTO() const {
  return Config.options().traceLTO();
}

std::string plugin::LinkerConfig::getMapFileName() const {
  return Config.options().layoutFile();
}

plugin::InputFile plugin::InputSymbol::getInputFile() const {
  if (!SymInfo)
    return plugin::InputFile(nullptr);
  // FIXME: Perhaps change plugin::InputFile to store const InputFile *
  return plugin::InputFile(
      const_cast<eld::InputFile *>(SymInfo->getInputFile()));
}

uint64_t InputSymbol::getSymbolIndex() const {
  if (!Sym)
    return 0;
  return Sym->getSymbolIndex();
}

bool InputSymbol::isUndef() const {
  if (!SymInfo)
    return false;
  return SymInfo->getSymbolSectionIndexKind() ==
         eld::SymbolInfo::SectionIndexKind::Undef;
}

bool InputSymbol::isCommon() const {
  if (!SymInfo)
    return false;
  return SymInfo->getSymbolSectionIndexKind() ==
         eld::SymbolInfo::SectionIndexKind::Common;
}

eld::ResolveInfo *InputSymbol::getResolveInfo() {
  if (!Sym)
    return nullptr;
  return Sym->resolveInfo();
}

plugin::Symbol plugin::Stub::getTargetSymbol() const {
  if (!BI)
    return plugin::Symbol(nullptr);
  auto stub = BI->stub();
  ASSERT(stub, "stub must never be null!");
  return plugin::Symbol(stub->savedSymInfo());
}

plugin::Symbol plugin::Stub::getStubSymbol() const {
  if (!BI)
    return plugin::Symbol(nullptr);
  auto stub = BI->stub();
  ASSERT(stub, "stub must never be null!");
  return plugin::Symbol(stub->symInfo());
}
