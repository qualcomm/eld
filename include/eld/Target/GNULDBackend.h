//===- GNULDBackend.h------------------------------------------------------===//
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
#ifndef ELD_TARGET_GNULDBACKEND_H
#define ELD_TARGET_GNULDBACKEND_H
#include "eld/GarbageCollection/GarbageCollection.h"
#include "eld/LayoutMap/LayoutInfo.h"
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
#include "eld/Input/ELFDynObjectFile.h"
#endif
#include "eld/Object/ObjectBuilder.h"
#include "eld/Readers/CommonELFSection.h"
#include "eld/Readers/ELFExecObjParser.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Readers/SymDefReader.h"
#include "eld/Script/Assignment.h"
#include "eld/Script/VersionScript.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "eld/Writers/ELFObjectWriter.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include <optional>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace eld {

class BuildIDFragment;
class BinaryFileParser;
class BitcodeReader;
class BranchIslandFactory;
class EhFrameHdr;
class EhFrameHdrSection;
class ELFDynamic;
class ELFDynObjFileFormat;
class ELFExecFileFormat;
class ELFFileFormat;
class ELFObjectFile;
class ELFObjectFileFormat;
class ELFSegmentFactory;
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
class GNUVerDefFragment;
class GNUVerNeedFragment;
#endif
class TargetInfo;
class Layout;
class LinkerConfig;
class LinkerScript;
class MemoryDesc;
class Module;
class ArchiveParser;
class ELFDynObjParser;
class ELFRelocObjParser;
class ELFExecObjParser;
class Relocation;
class ScriptMemoryRegion;
class StubFactory;
class SymDefReader;
class TimingFragment;

/** \class GNULDBackend
 *  \brief GNULDBackend provides a common interface for all GNU Unix-OS
 *  LDBackend.
 */
class GNULDBackend {
public:
  // Support for Fill Patterns.
  typedef struct PaddingT {
    Expression *Exp = nullptr;
    int64_t startOffset = -1;
    int64_t endOffset = -1;
  } PaddingT;

  struct SectionOffset {
    ELFSection *sec = nullptr;
    uint64_t offset = 0;
  };

  typedef std::tuple<ResolveInfo::Type, uint64_t, InputFile *, bool> SymDefInfo;

  // Based on Kind in LDFileFormat to define basic section orders for ELF,
  // and refer gold linker to add more enumerations to handle Regular and
  // BSS kind
  enum SectionOrder {
    SHO_nullptr = 0,     // nullptr
    SHO_GROUP = 1,       // .group
    SHO_INTERP,          // .interp
    SHO_START,           // .start
    SHO_RO_NOTE,         // .note.ABI-tag, .note.gnu.build-id
    SHO_NAMEPOOL,        // *.hash, .dynsym, .dynstr
    SHO_RELOCATION,      // .rel.*, .rela.*
    SHO_REL_PLT,         // .rel.plt should come after other .rel.*
    SHO_INIT,            // .init
    SHO_PLT,             // .plt
    SHO_TEXT,            // .text
    SHO_FINI,            // .fini
    SHO_RO,              // .rodata
    SHO_EXCEPTION,       // .eh_frame_hdr, .eh_frame, .gcc_except_table
    SHO_TLS_DATA,        // .tdata
    SHO_TLS_BSS,         // .tbss
    SHO_RELRO_LOCAL,     // .data.rel.ro.local
    SHO_RELRO,           // .data.rel.ro,
    SHO_RELRO_LAST,      // last of relro.
    SHO_NON_RELRO_FIRST, // for x86 to adjust .got.plt if needed
    SHO_DATA,            // .data
    SHO_LARGE_DATA,      // .ldata
    SHO_RW_NOTE,         //
    SHO_SMALL_DATA,      // .sdata
    SHO_SMALL_BSS,       // .sbss
    SHO_BSS,             // .bss
    SHO_LARGE_BSS,       // .lbss
    SHO_UNDEFINED,       // default order
    SHO_SHSTRTAB = 0xF0, // .shstrtab
    SHO_SYMTAB,          // .symtab
    SHO_SYMTAB_SHNDX,    // .symtab_shndxr
    SHO_STRTAB           // .strtab
  };

  // The type of dynamic relocation supported by all backends.
  enum DynRelocType : uint8_t {
    DEFAULT,
    GLOB_DAT,
    JMP_SLOT,
    RELATIVE,
    WORD_DEPOSIT,
    TLSDESC_GLOBAL,
    TLSDESC_LOCAL,
    DTPMOD_LOCAL,
    DTPMOD_GLOBAL,
    DTPREL_LOCAL,
    DTPREL_GLOBAL,
    TPREL_LOCAL,
    TPREL_GLOBAL
  };

  GNULDBackend(Module &pModule, TargetInfo *pInfo);

  virtual ~GNULDBackend();

  // -----  output sections  ----- //
  /// initStdSections - initialize standard sections of the output file.
  virtual eld::Expected<void> initStdSections();

  virtual ELFFileFormat *getOutputFormat() const;

  Module &getModule() const { return m_Module; }

  bool hasFatalError() const;

  LinkerConfig &config() const;

  // adds dummy fragment with proper size note.qc.timing section
  void insertTimingFragmentStub();

  TimingFragment *getTimingFragment() const { return m_pTimingFragment; }

  // -----  target symbols ----- //
  /// initStandardSymbols - initialize standard symbols.
  /// Some section symbols is undefined in input object, and linkers must set
  /// up its value. Take __init_array_begin for example. This symbol is an
  /// undefined symbol in input objects. ObjectLinker must finalize its value
  /// to the begin of the .init_array section, then relocation enties to
  /// __init_array_begin can be applied without emission of "undefined
  /// reference to `__init_array_begin'".
  virtual bool initStandardSymbols();

  /// finalizeSymbol - Linker checks pSymbol.reserved() if it's not zero,
  /// then it will ask backend to finalize the symbol value.
  /// @return ture - if backend set the symbol value sucessfully
  /// @return false - if backend do not recognize the symbol
  virtual bool finalizeSymbols() {
    return (finalizeStandardSymbols() && finalizeTargetSymbols());
  }

  /// finalizeStandardSymbols - set the value of standard symbols
  virtual bool finalizeStandardSymbols();

  /// finalizeTargetSymbols - set the value of target symbols
  virtual bool finalizeTargetSymbols() = 0;

  /// finalizeTLSSymbol - set the value of a TLS symbol
  virtual uint64_t finalizeTLSSymbol(LDSymbol *pSymbol);

  virtual size_t sectionStartOffset() const;

  TargetInfo &getInfo() const { return *m_pInfo; }

  virtual Relocator *getRelocator() const = 0;

  bool hasTextRel() const { return m_bHasTextRel; }

  bool hasStaticTLS() const { return m_bHasStaticTLS; }

  virtual uint64_t getSectLink(const ELFSection *S) const;

  /// readSection - read a target dependent section
  virtual bool readSection(InputFile &pInput, ELFSection *S);

  virtual void mayWarnSection(ELFSection *section) const;

  /// sizeShstrtab - compute the size of .shstrtab
  void sizeShstrtab();

  bool SetSymbolsToBeExported();

  virtual void sizeDynNamePools();

  virtual void initSymTab();

  virtual void sizeSymTab();

  virtual void sizeDynamic();

  virtual void finalizeBeforeWrite();

  /// emitSection - emit target-dependent section data
  virtual eld::Expected<uint64_t> emitSection(ELFSection *pSection,
                                              MemoryRegion &pRegion) const;

  /// emitRegNamePools - emit regular name pools - .symtab, .strtab
  virtual eld::Expected<void> emitRegNamePools(llvm::FileOutputBuffer &pOutput);

  /// emitNamePools - emit dynamic name pools - .dyntab, .dynstr, .hash
  virtual bool emitDynNamePools(llvm::FileOutputBuffer &pOutput);

  void setHasStaticTLS(bool pVal = true) { m_bHasStaticTLS = pVal; }

  /// getSectionOrder - compute the layout order of the section
  /// Layout calls this function to get the default order of the pSectHdr.
  /// If the pSectHdr.type() is LDFileFormat::Target, then getSectionOrder()
  /// will call getTargetSectionOrder().
  ///
  /// If targets favors certain order for general sections, please override
  /// this function.
  ///
  /// @see getTargetSectionOrder
  virtual unsigned int getSectionOrder(const ELFSection &pSectHdr) const;

  /// getTargetSectionOrder - compute the layout order of target section
  /// If the target favors certain order for the given gSectHdr, please
  /// override this function.
  ///
  /// By default, this function returns the maximun order, and pSectHdr
  /// will be the last section to be laid out.
  virtual unsigned int getTargetSectionOrder(const ELFSection &pSectHdr) const {
    return (unsigned int)-1;
  }

  /// elfSegmentTable - return the reference of the elf segment table
  ELFSegmentFactory &elfSegmentTable() const;

  /// commonPageSize - the common page size of the target machine
  virtual uint64_t commonPageSize() const;

  /// abiPageSize - the abi page size of the target machine
  virtual uint64_t abiPageSize() const;

  int64_t getSectionIdx(ELFSection *S) const;

  /// getSymbolIdx - get the symbol index of output static symbol table
  size_t getSymbolIdx(LDSymbol *pSymbol, bool IgnoreUnknown = false) const;

  /// getDynSymbolIdx - get the symbol index of output dynamic symbol table
  size_t getDynSymbolIdx(LDSymbol *pSymbol) const;

  /// Allocates common symbols. More precisely, create a section and
  /// a fragment for each common symbol stored in 'm_Module.m_CommonSymbols'.
  ///
  /// Each common symbol is allocated to a different internal input section
  /// of the name 'COMMON.${CommonSymbolName}'. No common symbols are
  /// allocated to the same internal input section. All common internal
  /// input sections are part of 'CommonSymbols' internal input file.
  virtual bool allocateCommonSymbols();

  virtual bool DoesOverrideMerge(ELFSection *pInputSection) const {
    return false;
  }

  /// merge Input Sections
  virtual ELFSection *mergeSection(ELFSection *pInputSection) {
    return nullptr;
  }

  /// setUpReachedSectionsForGC - set the reference between two sections for
  /// some special target sections. GC will set up the reference for the Regular
  /// and BSS sections. Backends can also set up the reference if need.
  virtual void setUpReachedSectionsForGC(
      GarbageCollection::SectionReachedListMap &pSectReachedListMap) const {}

  /// readRelocation - read ELF32_Rel entry
  virtual bool readRelocation(const llvm::ELF::Elf32_Rel &pRel,
                              Relocation::Type &pType, uint32_t &pSymIdx,
                              uint32_t &pOffset) const;

  /// readRelocation - read ELF32_Rela entry
  virtual bool readRelocation(const llvm::ELF::Elf32_Rela &pRel,
                              Relocation::Type &pType, uint32_t &pSymIdx,
                              uint32_t &pOffset, int32_t &pAddend) const;

  /// readRelocation - read ELF64_Rel entry
  virtual bool readRelocation(const llvm::ELF::Elf64_Rel &pRel,
                              Relocation::Type &pType, uint32_t &pSymIdx,
                              uint64_t &pOffset) const;

  /// readRel - read ELF64_Rela entry
  virtual bool readRelocation(const llvm::ELF::Elf64_Rela &pRel,
                              Relocation::Type &pType, uint32_t &pSymIdx,
                              uint64_t &pOffset, int64_t &pAddend) const;

  /// symbolNeedsPLT - return whether the symbol needs a PLT entry
  bool symbolNeedsPLT(const ResolveInfo &pSym) const;

  /// symbolNeedsCopyReloc - return whether the symbol needs a copy relocation
  bool symbolNeedsCopyReloc(const Relocation &pReloc,
                            const ResolveInfo &pSym) const;

  /// symbolNeedsDynRel - return whether the symbol needs a dynamic relocation
  bool symbolNeedsDynRel(const ResolveInfo &pSym, bool pSymHasPLT,
                         bool isAbsReloc) const;

  /// isSymbolPreemtible - whether the symbol can be preemted by other
  /// link unit
  bool isSymbolPreemptible(const ResolveInfo &pSym) const;

  /// symbolHasFinalValue - return true if the symbol's value can be decided at
  /// link time
  bool symbolFinalValueIsKnown(const ResolveInfo &pSym) const;

  bool canIssueUndef(const ResolveInfo *pSym);

  virtual ResolveInfo::Desc getSymDesc(uint32_t pShndx) const {
    return ResolveInfo::Define;
  }

  /// image base
  uint64_t getImageBase(bool HasInterp, bool LoadEHdr) const;

  /// getEntry - get the entry point name
  llvm::StringRef getEntry() const;

  /// getEntrySymbol - get the entry point symbol
  const LDSymbol *getEntrySymbol() const;

  //  -----  relaxation  -----  //
  /// initBRIslandFactory - initialize the branch island factory for relaxation
  virtual bool initBRIslandFactory() { return true; }

  virtual bool initTargetStubs() { return true; }

  /// initStubFactory - initialize the stub factory for relaxation
  virtual bool initStubFactory() { return true; }

  /// getBRIslandFactory
  virtual BranchIslandFactory *getBRIslandFactory() {
    return m_pBRIslandFactory;
  }

  /// getStubFactory
  virtual StubFactory *getStubFactory() { return m_pStubFactory; }

  virtual StubFactory *getStubFactory() const { return m_pStubFactory; }

  /// maxBranchOffset - return the max (forward) branch offset of the backend.
  /// Target can override this function if needed.
  virtual uint64_t maxBranchOffset() { return (uint64_t)-1; }

  /// checkAndSetHasTextRel - check pSection flag to set HasTextRel
  void checkAndSetHasTextRel(const ELFSection &pSection);

  /// sortRelocation - sort the dynamic relocations to let dynamic linker
  /// process relocations more efficiently
  virtual void sortRelocation(ELFSection &pSection);

  virtual bool maySkipRelocProcessing(Relocation *Reloc) const;

  virtual int64_t getPLTAddr(ResolveInfo *pInfo) const;

  virtual Stub *getBranchIslandStub(Relocation *pReloc,
                                    int64_t pTargetValue) const = 0;

  MemoryRegion getFileOutputRegion(llvm::FileOutputBuffer &pBuffer,
                                   size_t pOffset, size_t pLength);

  // Set the TLS size.
  static void setTLSTemplateSize(uint64_t sz) { m_TLSBaseSize = sz; }

  static uint64_t getTLSTemplateSize() { return m_TLSBaseSize; }

  virtual int32_t getPacketOffset(const Relocation &pReloc) const { return 0; }

  std::string getOutputRelocSectName(std::string baseName, uint32_t type) {
    return (type == llvm::ELF::SHT_RELA ? ".rela" : ".rel") + baseName;
  }

  /// LTO Flow Setup
  virtual bool ltoNeedAssembler() { return false; }

  virtual bool ltoCallExternalAssembler(const std::string &Input,
                                        std::string RelocModel,
                                        const std::string &Output) {
    return false;
  }

  virtual void AddLTOOptions(std::vector<std::string> &) {}

  virtual void fillValuesFromUser(llvm::FileOutputBuffer &pOutput);

  void fillRegion(MemoryRegion &mr, const std::vector<PaddingT> &FillV) const;

  void maybeFillRegion(const OutputSectionEntry *O, MemoryRegion R) const;

  virtual bool initRelocator() = 0;

  void createInternalInputs();

  virtual void initTargetSections(ObjectBuilder &pBuilder) = 0;

  ELFObjectFile *getDynamicSectionHeadersInputFile() const {
    return m_DynamicSectionHeadersInputFile;
  }

  virtual void initDynamicSections(ELFObjectFile &) {}

  virtual void initTargetSymbols() = 0;

  virtual void initPatchSections(ELFObjectFile &) {}

  /// getRelEntrySize - the size in BYTE of rel type relocation
  virtual size_t getRelEntrySize() = 0;

  /// getRelEntrySize - the size in BYTE of rela type relocation
  virtual size_t getRelaEntrySize() = 0;

  virtual Relocation::Type getCopyRelType() const;

  uint64_t getSymbolSize(LDSymbol *pSymbol) const;

  uint64_t getSymbolInfo(LDSymbol *pSymbol) const;

  uint64_t getSymbolValue(LDSymbol *pSymbol) const;

  std::pair<uint16_t, uint32_t> getSymbolShndx(LDSymbol *pSymbol) const;

  /// emitSymbol32 - emit an ELF32 symbol
  void emitSymbol32(llvm::ELF::Elf32_Sym &pSym32, LDSymbol *pSymbol,
                    char *pStrtab, size_t pStrtabsize, size_t pSymtabIdx,
                    bool isDynSymTab);

  /// emitSymbol64 - emit an ELF64 symbol
  void emitSymbol64(llvm::ELF::Elf64_Sym &pSym64, LDSymbol *pSymbol,
                    char *pStrtab, size_t pStrtabsize, size_t pSymtabIdx,
                    bool isDynSymTab);

  /// createProgramHdrs - base on output sections to create the program headers
  bool createProgramHdrs();

  /// createScriptProgramHdrs - Create program headers mentioned in Linker
  /// Script
  bool createScriptProgramHdrs();

  bool assignOffsets(uint64_t Offset);

  void evaluateAssignments(OutputSectionEntry *output, uint32_t &atIndex);

  void evaluateAssignmentsAtEndOfOutputSection(OutputSectionEntry *output);

  // Print padding between end and start fragments of adjacent rules
  std::vector<PaddingT>
  getPaddingBetweenFragments(ELFSection *section, const Fragment *StartFrag,
                             const Fragment *EndFrag) const;

  /// createScriptProgramHdrs - Create program headers mentioned in Linker
  /// Script
  bool createSegmentsFromLinkerScript();

  virtual void initSegmentFromLinkerScript(ELFSegment *pSegment) {}

  /// setupOutputAddresses - assign sections addresses/offsets when using pre-
  /// defiend program headers from LinkerScript
  void setupOutputAddresses();

  /// doCreateProgramHdrs - backend can implement this function to create the
  /// target-dependent segments
  virtual void doCreateProgramHdrs() {}

  /// Evaluate target symbols before relaxation.
  virtual void evaluateTargetSymbolsBeforeRelaxation() {}

  /// setupProgramHdrs - set up the attributes of segments
  ///  (i.e., offset, addresses, file/mem size, flag,  and alignment)
  bool setupProgramHdrs();

  /// getSegmentFlag - give a section flag and return the corresponding segment
  /// flag
  inline uint32_t getSegmentFlag(const uint32_t pSectionFlag);

  virtual bool finalizeScanRelocations() { return true; }

  /// setOutputSectionOffset - helper function to set output sections' offset.
  bool setOutputSectionOffset();

  /// placeOutputSections - place output sections based on SectionMap
  bool placeOutputSections();

  /// layout - layout method
  bool layout();

  /// preLayout - Backend can do any needed modification before layout
  void preLayout();

  /// postLayout -Backend can do any needed modification after layout
  bool postLayout();

  void printCref(bool pLTOPhase) const;

  /// printLayout
  bool printLayout();

  /// preLayout - Backend can do any needed modification before layout
  virtual void doPreLayout() {}

  /// postLayout -Backend can do any needed modification after layout
  virtual void doPostLayout();

  /// postProcessing - Backend can do any needed modification in the final stage
  virtual eld::Expected<void> postProcessing(llvm::FileOutputBuffer &pOutput);

  /// dynamic - the dynamic section of the target machine.
  virtual ELFDynamic *dynamic() = 0;

  /// relax - the relaxation pass
  virtual bool relax();

  /// mayBeRelax - Backend can orevride this function to add its relaxation
  /// implementation. Set pFinished to true if no more passes are needed.
  virtual void mayBeRelax(int pass, bool &pFinished) { pFinished = true; }

  virtual void setOptions() {}

  void assignOffsetsToSkippedSections();

  virtual bool checkForLinkerScriptPhdrErrors() const;

  virtual void initializeAttributes() {}

  virtual uint64_t getValueForDiscardedRelocations(const Relocation *R) const {
    return 0;
  }

  virtual bool hasSymInfo(const Relocation *X) const { return true; }

  virtual DynRelocType getDynRelocType(const Relocation *X) const {
    return DEFAULT;
  }
  LDSymbol &defineSymbolforCopyReloc(eld::IRBuilder &pBuilder,
                                     ResolveInfo *pSym, ResolveInfo *origSym);

  // Find segment with a name segmentName.
  ELFSegment *findSegment(std::string segmentName) const {
    auto seg = _segments.find(segmentName);
    if (seg == _segments.end())
      return nullptr;
    return seg->getValue();
  }

  std::unordered_map<const ResolveInfo *, VersionSymbol *> &symbolScopes() {
    return SymbolScopes;
  }

  const std::unordered_map<const ResolveInfo *, VersionSymbol *> &
  symbolScopes() const {
    return SymbolScopes;
  }

  void addSymbolScope(ResolveInfo *R, VersionSymbol *V) { SymbolScopes[R] = V; }

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  VersionSymbol *getSymbolScope(const ResolveInfo *R) const {
    auto it = SymbolScopes.find(R);
    if (it != SymbolScopes.end())
      return it->second;
    return nullptr;
  }
#endif

  std::vector<Relocation *> &getInternalRelocs() { return m_InternalRelocs; }

  // If the backend needs to take care of clearing any data structures or
  // inspecting them, this is the last chance before merging.
  virtual void finishAssignOutputSections() { return; }

  // Return if the Targets have updated some section properties, due to which
  // the linker needs to REDO assigning addresses.
  virtual bool updateTargetSections() { return false; }

  // The target can decide on how we want to handle BSS sections.
  // If the linker is emitting a region table, its possible to mix BSS
  // DATA in one segment.
  virtual bool handleBSS(const ELFSection *prev, const ELFSection *cur) const {
    return (prev->isNoBits() && !cur->isNoBits());
  }

  bool RunPluginsAndProcessHelper(SectionMap::OutputSectionEntryDescList &M,
                                  bool MatchSections = false);

  bool InitializePlugins(SectionMap::OutputSectionEntryDescList &O);

  bool CleanupPlugins(SectionMap::OutputSectionEntryDescList &O);

  bool RunVAPluginsAndProcess(SectionMap::OutputSectionEntryDescList &O);

  void pluginLinkSections(OutputSectionEntry *A, OutputSectionEntry *B);

  // -----------------Support for Provide-----------------------------
  void addProvideSymbol(llvm::StringRef symName, Assignment *provideCmd) {
    ASSERT(!ProvideMap.count(symName),
           "Provide symbol of the same name already exists!");
    ProvideMap[symName] = provideCmd;
  }

  bool isSymInProvideMap(llvm::StringRef symName) {
    return ProvideMap.count(symName);
  }

  void addSymDefProvideSymbol(llvm::StringRef symName,
                              ResolveInfo::Type resolverType, uint64_t symVal,
                              InputFile *file, bool isPatchable = false) {
    if (!m_SymDefProvideMap.count(symName))
      m_SymDefProvideMap[symName] =
          std::make_tuple(resolverType, symVal, file, isPatchable);
  }

  LDSymbol *canProvideSymbol(ResolveInfo *R);

  LDSymbol *canProvideSymbol(llvm::StringRef name);
  // ----------------Linker emitted comment section ---------------------
  virtual void makeVersionString();

  // ------------------- GOT/PLT sections -------------------------------
  ELFSection *getGOT() const;
  ELFSection *getGOTPLT() const;
  ELFSection *getPLT() const;
  ELFSection *getRelaDyn() const;
  ELFSection *getRelaPLT() const;

  virtual LDSymbol *getGOTSymbol() const { return m_pGOTSymbol; }

  void recordRelativeReloc(Relocation *R, const Relocation *N) {
    m_RelativeRelocMap[R] = N;
  }

  // Patching sections.
  ELFSection *getGOTPatch() const;
  ELFSection *getRelaPatch() const;

  // -----------------Segment Size Helper --------------------------------
  bool isOffsetAssigned() const { return m_OffsetsAssigned; }

  bool setupSegment(ELFSegment *E);

  // Setup the segment Offset with 'S' as the section previous to the section
  // BeginOffset the start Offset of the segment, and Check is set based on
  // whether we want to check for Compact semantics or anything else in the
  // future.
  std::pair<int64_t, ELFSection *> setupSegmentOffset(ELFSegment *E,
                                                      ELFSection *S = nullptr,
                                                      int64_t BeginOffset = 0,
                                                      bool Check = false);

  // ----------------------PHDR/FILEHDR support----------------------------

  void clearSegmentOffset(ELFSegment *S);

  virtual bool validateArchOpts() const { return true; }

  /// Handle a target-specific relocation. Return true if a relocation was
  /// handled here, false otherwise.
  virtual bool handleRelocation(ELFSection *pSection, Relocation::Type pType,
                                LDSymbol &pSym, uint32_t pOffset,
                                Relocation::Address pAddend = 0,
                                bool pLastVisit = false) {
    return false;
  }

  virtual bool handlePendingRelocations(ELFSection *S) { return true; }

  virtual bool shouldIgnoreRelocSync(Relocation *Reloc) const { return false; }

  // Sometimes an internal value is stored in Relocation::Type (RISC-V
  // Vendor/Internal relocations), This maps the internal relocation type back
  // to the external relocation type it came from, for emit-relocs.
  virtual Relocation::Type
  getRemappedInternalRelocationType(Relocation::Type t) const {
    return t;
  }

  // ------------------- EhFrame Hdr -------------------------------
  EhFrameHdrSection *getEhFrameHdr() const { return m_pEhFrameHdrSection; }

  void createEhFrameFillerAndHdrSection();

  // ------------------------- EhFrame Hdr support ----------------
  bool hasEhFrameHdr() const { return m_pEhFrameHdrFragment != nullptr; }

  void populateEhFrameHdrWithNoFDEInfo() { m_EhFrameHdrContainsTable = false; }

  // Relocator support for accessing Section Information from Linker script
  // symbol.
  ELFSection *getSectionInfo(LDSymbol *symbol) const;

  // ------------------------- Get Segments for Section ----------------
  const std::vector<ELFSegment *>
  getSegmentsForSection(const OutputSectionEntry *O) const;

  // -----------------Decide to emit symbol in the Output -------------------
  virtual bool addSymbolToOutput(ResolveInfo *pInfo) { return true; }

  // --------------------Input file special processing -------------------
  virtual bool processInputFiles(std::vector<InputFile *> &Inputs) {
    return true;
  }

  // ----------------provide standard symbol support ----------------
  bool isStandardSymbol(llvm::StringRef Symbol) const;

  void setStandardSymbol(llvm::StringRef Symbol, ResolveInfo *R);

  ResolveInfo *getStandardSymbol(llvm::StringRef Symbol);

  // ---------------- magic symbol support ----------------
  // Test if the SymbolName refers to a magic section symbol. If yes, return the
  // section name. Otherwise, return an empty string.
  static llvm::StringRef parseSectionMagicSymbol(llvm::StringRef SymbolName);

  static bool isSectionMagicSymbol(llvm::StringRef pName);

  // Scan global symbols after layout and define section magic symbols.
  bool defineStandardAndSectionMagicSymbols();

  // -----------------------Backend symbols support ----------------
  virtual bool addSymbols() { return true; }

  // -------------------- Finalize Layout callback ----------------
  virtual bool finalizeLayout() { return true; }

  // --- Exclude symbol support
  void markSymbolForRemoval(const ResolveInfo *S);

  // --- Support Relaxed Relocations --------------
  virtual bool isRelocationRelaxed(Relocation *R) const { return false; }

  // --- Get the symbol address for GOT -------------
  virtual size_t getGOTSymbolAddr() const;

  /// Returns true if file header is loaded.
  bool isFileHeaderLoaded() const {
    if (!m_ehdr || !isEhdrInLayout())
      return false;
    return isOutputSectionInLoadSegment(m_ehdr->getOutputSection());
  }

  /// Returns true if PHDRS is loaded.
  bool isPHDRSLoaded() const {
    if (!m_phdr || !isPhdrInLayout())
      return false;
    return isOutputSectionInLoadSegment(m_phdr->getOutputSection());
  }

  bool isOutputSectionInLoadSegment(OutputSectionEntry *OSE) const;

  const ELFSegment *
  getLoadSegmentForOutputSection(OutputSectionEntry *OSE) const;

  ELFSection *getEhdr() const { return m_ehdr; }

  ELFSection *getPhdr() const { return m_phdr; }

  /// Returns the common symbol associated with the section 'commonSection'.
  LDSymbol *getCommonSymbol(const CommonELFSection *commonSection) const;

  /// Returns the image start virtual memory address.
  uint64_t getImageStartVMA() const;

  bool hasImageStartVMA() const { return m_ImageStartVMA.has_value(); }

  // Allow backends to override sections that are processed for garbage
  // collection
  virtual std::optional<bool>
  shouldProcessSectionForGC(const ELFSection &S) const {
    return std::nullopt;
  }

  // -------------------------------MEMORY Support ------------------------
  eld::Expected<bool> createMemoryRegions();

  eld::Expected<bool> addScriptMemoryRegion(MemoryDesc &memDesc);

  /// Returns number of symbols in the PLT.
  virtual std::size_t PLTEntriesCount() const = 0;

  /// Returns number of symbols in the GOT.
  virtual std::size_t GOTEntriesCount() const = 0;

  /// Returns number of NOLOAD sections.
  std::size_t NOLOADSectionsCount() const { return noLoadSections.size(); }

  // ----------------------- Provide Symbols Support --------------------
  void provideSymbols();

  virtual void setDefaultConfigs();

  // ----------------------- BuildID Support --------------------
  eld::Expected<void> finalizeAndEmitBuildID(llvm::FileOutputBuffer &pOutput);

  void setNeedEhdr() { m_NeedEhdr = true; }

  void setNeedPhdr() { m_NeedPhdr = true; }

  bool isEhdrNeeded() const { return m_NeedEhdr; }

  bool isPhdrNeeded() const { return m_NeedPhdr; }

  // ----------------------- Patching -----------------------------------
  // Absolute PLTs are used to redirect symbols in patch builds.
  void recordAbsolutePLT(ResolveInfo *, const ResolveInfo *);

  const ResolveInfo *findAbsolutePLT(ResolveInfo *I) const;

  // Symbol versioning helpers
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void initSymbolVersioningSections();
  ELFSection *getGNUVerSymSection() const { return GNUVerSymSection; }
  ELFSection *getGNUVerDefSection() const { return GNUVerDefSection; }
  GNUVerDefFragment *getGNUVerDefFragment() const { return GNUVerDefFrag; }
  ELFSection *getGNUVerNeedSection() const { return GNUVerNeedSection; }
  GNUVerNeedFragment *getGNUVerNeedFragment() const { return GNUVerNeedFrag; }
  void setShouldEmitVersioningSections(bool Should) {
    ShouldEmitVersioningSections = Should;
  }
  bool shouldEmitVersioningSections() const {
    return ShouldEmitVersioningSections;
  }
  std::optional<uint16_t> getSymbolVersionID(const ResolveInfo *R) const {
    auto it = OutputVersionIDs.find(R);
    if (it == OutputVersionIDs.end())
      return std::nullopt;
    return it->second;
  }
#endif

protected:
  virtual int numReservedSegments() const { return m_NumReservedSegments; }

  void DefineStandardSymFromSegment(llvm::StringRef Start, llvm::StringRef SymB,
                                    uint32_t IncludePermissions,
                                    uint32_t ExcludePermissions,
                                    int SymBAlign = 0, bool isBSS = false,
                                    bool searchBackwards = false,
                                    uint32_t SegType = llvm::ELF::PT_LOAD);

  void setStartOffset(int64_t Off) { m_StartOffset = Off; }

  // ------------------ Target Specific segment support ------------
  virtual void addTargetSpecificSegments();

  void addSectionInfo(LDSymbol *symbol, ELFSection *section);

  /// Returns the name of the common symbol associated with the section
  /// 'commonSection'.
  std::string getCommonSymbolName(const CommonELFSection *commonSection) const;

  /// FIXME: This is not implemented anywhere
  bool evaluateOutputSectionDataCmds();

private:
  uint32_t getOneEhdrSize() const;

  uint32_t getOnePhdrSize() const;

  // Utility function to determine which assignments can be skipped.
  bool shouldskipAssert(const Assignment *Assign) const;

  bool checkBssMixing(const ELFSegment &Seg) const;

  bool checkCrossReferences();

  void checkCrossReferencesHelper(InputFile *input);

  bool canSkipSymbolFromExport(ResolveInfo *R, bool isEntry = false) const;

  static uint64_t m_TLSBaseSize;

  void changeSymbolsFromAbsoluteToGlobal(OutputSectionEntry *out);

  /// When there is a SECTIONS command, then:
  ///   - When afterLayout is false, only evaluate Assignment::BEFORE_SECTIONS.
  ///   - When afterLayout is true, only evaluate Assignment::AFTER_SECTIONS.
  /// When there is no SECTIONS command, then:
  ///   - Evaluate ASSIGNMENT::BEFORE_SECTIONS regardless of afterLayout value.
  ///     This behavior should be improved.
  void evaluateScriptAssignments(bool afterLayout);

  bool isRelROSection(const ELFSection *sect) const;

  bool applyVersionScriptScopes();

  bool isGOTAndGOTPLTMerged() const;

  void ConvertWeakUndefs();

  // --------------------- PHDRS/FILEHDR support -----------------

  // This function appends contents of FILE header / PHDR's into the
  // output section layout.
  bool addPhdrsIfNeeded();

  void createFileHeader();

  void createProgramHeader();

  void addFileHeaderToLayout();

  void addProgramHeaderToLayout();

  bool TryToPlaceAtSection(RuleContainer *In, Fragment *Frag,
                           ELFSection *Section, uint32_t Index);

  bool InsertAtSectionToEnd(ELFSection *OutSection, uint64_t &Offset,
                            RuleContainer *CurRule, RuleContainer *NextRule,
                            Expression *Fill, uint32_t Index);

  // ----------------------Handle Assert --------------------------
  void evaluateAsserts();

  // ----------------------- Unique output section support --------------------
  ELFSection *getOutputRelocationSection(ELFSection *S, ELFSection *rs);

  // ----------------------- MEMORY Support --------------------
  void clearMemoryRegions();

  void verifyMemoryRegions();

  eld::Expected<void> printMemoryRegionsUsage();

  bool assignMemoryRegions();

  // ---------------------------Section Overlap----------------------------
  void checkOverlap(llvm::StringRef name, std::vector<SectionOffset> &sections,
                    bool isVirtualAddr);

  std::string rangeToString(uint64_t addr, uint64_t len);

  /// Returns all the symbols on which the PROVIDE symbol 'symName' depends upon
  /// but which are not referenced anywhere in the user program.
  /// Indirectly reference here implies that the user-program indirectly depends
  /// upon these symbols.
  void findIndirectlyReferencedProvideSyms(
      llvm::StringRef symName,
      std::unordered_set<std::string> &indirectReferenceProvideSyms);

  /// Returns true if the provide symbol provided by the provideCmd is actually
  /// being used for the link.
  bool isProvideSymBeingUsed(const Assignment *provideCmd) const;

  // --------------------------Provide target specific symbol values -------
  void resolveTargetDefinedSymbols();

  bool verifySegments() const;

private:
  // Reserved segments.
  int32_t m_NumReservedSegments = 0;

  bool m_NewSectionsAddedToLayout = false;

  // By default we set file header and program header to not show up in the
  // image layout
  bool m_EhdrInLayout = false;

  bool m_PhdrInLayout = false;

  // Define standard and section magic symbols, if the relocation refers to one.
  void defineStandardAndSectionMagicSymbol(const ResolveInfo &ResolveInfo);

  // This is used as a communication mechanism between when a new section is
  // added to the image layout, causing image layout to resume from start
  // of the image
  bool isNewSectionsAddedToLayout() const;

  // This function is called when an orphan section is encountered.
  // It returns true if the orphan handling flag is set to "error"
  // and false otherwise. This function also emits appropriate diagnostics
  // in case the flag is set to "warn" or "error".
  bool handleOrphanSection(const ELFSection *elem) const;

  bool isSymbolStringTableSection(const ELFSection *elem) const;

  bool isNonDymSymbolStringTableSection(const ELFSection *elem) const;

  // This is used as a communication mechanism between when a new section is
  // added to the image layout, causing image layout to resume from start
  // of the image. This cause the state to be reset.
  void resetNewSectionsAddedToLayout();

  // This is used as a communication mechanism between when a new section is
  // added to the image layout, causing image layout to resume from start
  // of the image. This cause the image layout to be recomputed
  void setNewSectionsAddToLayout();

  // This function sets that the image layout contains file header
  void setEhdrInLayout() { m_EhdrInLayout = true; }

  // This function sets that the image layout contains program header
  void setPhdrInLayout() { m_PhdrInLayout = true; }

  // This function returns if the file header is present in
  // the output image layout
  bool isEhdrInLayout() const { return m_EhdrInLayout; }

  bool isPhdrInLayout() const { return m_PhdrInLayout; }

  // This function is the core function to set the file header and the program
  // header when loaded assign a proper virtual address
  bool allocateHeaders();

  // Setup TLS alignment and check for any layout issues
  bool setupTLS();

  /// Assigns the version IDs to the dynamic symbols.
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void assignOutputVersionIDs();
  void setSymbolVersionID(const ResolveInfo *R, uint16_t VerID) {
    OutputVersionIDs[R] = VerID;
  }
#endif

protected:
  Module &m_Module;

  ELFFileFormat *m_pFileFormat = nullptr;

  // TargetInfo
  TargetInfo *m_pInfo = nullptr;

  // ELF segment factory
  ELFSegmentFactory *m_pELFSegmentTable = nullptr;

  // branch island factory
  BranchIslandFactory *m_pBRIslandFactory = nullptr;

  // stub factory
  StubFactory *m_pStubFactory = nullptr;

  // map the LDSymbol to its index in the output static symbol table
  llvm::DenseMap<LDSymbol *, uint32_t> m_pSymIndexMap;

  // map the LDSymbol to its index in the output dynamic symbol table
  llvm::DenseMap<LDSymbol *, uint32_t> m_pDynSymIndexMap;

  // output section map
  std::unordered_map<ELFSection *, ELFSection *> m_OutputSectionMap;

  // section .eh_frame_hdr
  EhFrameHdrSection *m_pEhFrameHdrSection = nullptr;

  // EhFrameHdr fragment
  Fragment *m_pEhFrameHdrFragment = nullptr;

  // eh_frame filler section
  ELFSection *m_pEhFrameFillerSection = nullptr;

  // ----- dynamic flags ----- //
  // DF_TEXTREL of DT_FLAGS
  bool m_bHasTextRel = false;

  // DF_STATIC_TLS of DT_FLAGS
  bool m_bHasStaticTLS = false;

  // Has Offsets been assigned already for all segments ?
  bool m_OffsetsAssigned = false;

  // Does EhFrameHdr need to populate FDE information ?
  bool m_EhFrameHdrContainsTable = true;

  // Elf header.
  ELFSection *m_ehdr = nullptr;

  // Program header.
  ELFSection *m_phdr = nullptr;

  // SysV Hash
  ELFSection *m_pSysVHash = nullptr;

  // GNU Hash
  ELFSection *m_pGNUHash = nullptr;

  // Comment section.
  ELFSection *m_pComment = nullptr;

  // .note.qc.timing
  ELFSection *m_pTiming = nullptr;
  TimingFragment *m_pTimingFragment = nullptr;

  // NONE segment
  ELFSegment *m_pNONESegment = nullptr;

  // Build ID Section
  ELFSection *m_pBuildIDSection = nullptr;
  BuildIDFragment *m_pBuildIDFragment = nullptr;

  // At Table.
  uint32_t m_AtTableIndex = 0;

  // Start Offset.
  int64_t m_StartOffset = 0;

  std::vector<ResolveInfo *> DynamicSymbols;

  bool IsSectionTracingRequested = false;

  std::unordered_map<LDSymbol *, ELFSection *> m_SymbolToSection;

  llvm::StringMap<ELFSegment *> _segments;
  std::unordered_map<const OutputSectionEntry *, std::vector<ELFSegment *>>
      _segmentsForSection;
  // Section to a NOTE segment map.
  std::unordered_map<const OutputSectionEntry *, ELFSegment *>
      _noteSegmentsForSection;
  llvm::DenseMap<ELFSection *, std::vector<PaddingT>> m_PaddingMap;
  // Version script wildcard match will populate this scope
  std::unordered_map<const ResolveInfo *, VersionSymbol *> SymbolScopes;
  std::vector<ELFSection *> noLoadSections;
  // All provide symbols that may need to be defined.
  llvm::StringMap<Assignment *> ProvideMap;

  // All sym def provide symbols that may need to be defined.
  llvm::StringMap<SymDefInfo> m_SymDefProvideMap;
  std::vector<Relocation *> m_InternalRelocs;
  std::unordered_map<OutputSectionEntry *, std::vector<Fragment *>>
      OutputSectionToFrags;
  llvm::StringMap<ResolveInfo *> m_ProvideStandardSymbols;
  // Unique output section support
  std::unordered_map<ELFSection *, ELFSection *>
      m_RelocationSectionForOutputSection;
  // Exclude symbol support
  std::unordered_set<const ResolveInfo *> m_SymbolsToRemove;

  // Memory Region support
  std::unordered_map<std::string, ScriptMemoryRegion *> m_MemoryRegionMap;

  // Dynamic linking
  ELFObjectFile *m_DynamicSectionHeadersInputFile = nullptr;
  LDSymbol *m_pGOTSymbol = nullptr;
  llvm::DenseMap<Relocation *, const Relocation *> m_RelativeRelocMap;

  // Patching.
  llvm::DenseMap<ResolveInfo *, const ResolveInfo *> m_AbsolutePLTMap;

  std::optional<uint64_t> m_ImageStartVMA;

  llvm::DenseSet<OutputSectionEntry *> PluginLinkedSections;

  // Load file header and program header ?
  bool m_NeedEhdr = false;

  bool m_NeedPhdr = false;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  bool ShouldEmitVersioningSections = false;
  ELFSection *GNUVerSymSection = nullptr;
  ELFSection *GNUVerDefSection = nullptr;
  GNUVerDefFragment *GNUVerDefFrag = nullptr;
  ELFSection *GNUVerNeedSection = nullptr;
  GNUVerNeedFragment *GNUVerNeedFrag = nullptr;
  std::unordered_map<const ResolveInfo *, uint16_t> OutputVersionIDs;
#endif
};

} // namespace eld

#endif
