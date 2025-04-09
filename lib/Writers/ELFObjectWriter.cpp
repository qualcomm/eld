//===- ELFObjectWriter.cpp-------------------------------------------------===//
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
#include "eld/Writers/ELFObjectWriter.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/Fragment/StringFragment.h"
#include "eld/Fragment/Stub.h"
#include "eld/Fragment/TimingFragment.h"
#include "eld/Plugin/PluginManager.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/RegisterTimer.h"
#include "eld/Support/StringRefUtils.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/ELFSegment.h"
#include "eld/Target/ELFSegmentFactory.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/LDFileFormat.h"
#include "eld/Target/Relocator.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace eld;

//===----------------------------------------------------------------------===//
// ELFObjectWriter
//===----------------------------------------------------------------------===//
ELFObjectWriter::ELFObjectWriter(GNULDBackend &CurBackend,
                                 LinkerConfig &CurConfig)
    : Backend(CurBackend), Config(CurConfig) {}

ELFObjectWriter::~ELFObjectWriter() {}

eld::Expected<void> ELFObjectWriter::writeSection(
    Module &CurModule, llvm::FileOutputBuffer &CurOutput, ELFSection *Section) {
  MemoryRegion Region;
  // Request output region
  switch (Section->getKind()) {
  case LDFileFormat::Note:
    if (!Section->hasSectionData())
      return {};
    LLVM_FALLTHROUGH;
  case LDFileFormat::Regular:
  case LDFileFormat::Internal:
  case LDFileFormat::Common:
  case LDFileFormat::DynamicRelocation:
  case LDFileFormat::Group:
  case LDFileFormat::MetaData:
  case LDFileFormat::OutputSectData:
  case LDFileFormat::Relocation:
  case LDFileFormat::Target:
  case LDFileFormat::Debug:
  case LDFileFormat::MergeStr:
  case LDFileFormat::GCCExceptTable:
  case LDFileFormat::EhFrame:
  case LDFileFormat::Timing: {
    if (Section->getOutputELFSection()->isNoBits())
      return {};
    Region = Backend.getFileOutputRegion(
        CurOutput, Section->getOutputELFSection()->offset(),
        Section->getOutputELFSection()->size());
    if (Region.size() == 0) {
      return {};
    }
  } break;

  case LDFileFormat::Null:
  case LDFileFormat::NamePool:
  case LDFileFormat::Version:
  case LDFileFormat::StackNote:
  case LDFileFormat::Discard:
  case LDFileFormat::EhFrameHdr:
  case LDFileFormat::GNUProperty:
    // Ignore these sections
    return {};
  default:
    Config.raise(Diag::unsupported_section_kind)
        << Section->getKind() << Section->name();
    return {};
  }
  eld::Expected<void> ExpWrite = writeRegion(CurModule, Section, Region);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpWrite);
  return {};
}

eld::Expected<void> ELFObjectWriter::writeRegion(Module &CurModule,
                                                 ELFSection *Section,
                                                 MemoryRegion &Region) {
  if (Section->isNoBits())
    return {};
  if (Section->isIgnore())
    return {};
  if (Section->isDiscard())
    return {};
  // Write out sections with data
  switch (Section->getKind()) {
  case LDFileFormat::GCCExceptTable:
  case LDFileFormat::Regular:
  case LDFileFormat::Common:
  case LDFileFormat::Internal:
  case LDFileFormat::MetaData:
  case LDFileFormat::OutputSectData:
  case LDFileFormat::Debug:
  case LDFileFormat::Note:
  case LDFileFormat::MergeStr:
  case LDFileFormat::EhFrame:
  case LDFileFormat::Timing: {
    eld::Expected<void> ExpEmit = emitSection(Section, Region);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpEmit);
    break;
  }
  case LDFileFormat::DynamicRelocation: {
    // sort relocation for the benefit of the dynamic linker.
    target().sortRelocation(*Section);
    if (Config.targets().is32Bits())
      emitRelocation<llvm::object::ELF32LE>(CurModule, Section, Region, true);
    if (Config.targets().is64Bits())
      emitRelocation<llvm::object::ELF64LE>(CurModule, Section, Region, true);
    break;
  }
  case LDFileFormat::Relocation: {
    if (Config.targets().is32Bits())
      emitRelocation<llvm::object::ELF32LE>(CurModule, Section, Region, false);
    if (Config.targets().is64Bits())
      emitRelocation<llvm::object::ELF64LE>(CurModule, Section, Region, false);
    break;
  }
  case LDFileFormat::Target: {
    auto ExpEmit = target().emitSection(Section, Region);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpEmit);
  } break;
  case LDFileFormat::Group:
    emitGroup(Section, Region);
    break;
  default:
    llvm_unreachable("invalid section kind");
  }
  return {};
}

void ELFObjectWriter::writeLinkTimeStats(Module &CurModule,
                                         uint64_t BeginningOfTime,
                                         uint64_t Duration) {

  TimingFragment *F = CurModule.getBackend()->getTimingFragment();
  F->setData(BeginningOfTime, Duration);
}

std::error_code
ELFObjectWriter::writeObject(Module &CurModule,
                             llvm::FileOutputBuffer &CurOutput) {
  bool IsDynobj = Config.codeGenType() == LinkerConfig::DynObj;
  bool IsExec = Config.codeGenType() == LinkerConfig::Exec;
  bool IsBinary = Config.codeGenType() == LinkerConfig::Binary;
  bool IsObject = Config.codeGenType() == LinkerConfig::Object;

  assert(IsDynobj || IsExec || IsBinary || IsObject);

  if (IsDynobj || IsExec) {
    // Write out name pool sections: .dynsym, .dynstr, .hash
    eld::RegisterTimer T("Emit Dynamic Name Pool sections", "Emit Output File",
                         Config.options().printTimingStats());
    if (!target().emitDynNamePools(CurOutput))
      return make_error_code(std::errc::function_not_supported);
  }

  if (IsObject || IsDynobj || IsExec) {
    // Write out name pool sections: .symtab, .strtab
    auto E = target().emitRegNamePools(CurOutput);
    if (!E) {
      Config.getDiagEngine()->raiseDiagEntry(std::move(E.error()));
      return make_error_code(std::errc::no_such_file_or_directory);
    }
  }

  {
    PluginManager &PM = CurModule.getPluginManager();
    if (!PM.callActBeforeWritingOutputHook()) {
      // Return generic error-code. Actual error is already reported!
      return make_error_code(std::errc::not_supported);
    }
  }

  if (IsBinary) {
    // Iterate over the loadable segments and write the corresponding sections
    ELFSegmentFactory::iterator Seg, SegEnd = target().elfSegmentTable().end();

    for (Seg = target().elfSegmentTable().begin(); Seg != SegEnd; ++Seg) {
      if (llvm::ELF::PT_LOAD == (*Seg)->type()) {
        ELFSegment::iterator Sect, SectEnd = (*Seg)->end();
        for (Sect = (*Seg)->begin(); Sect != SectEnd; ++Sect) {
          eld::Expected<void> ExpWrite =
              writeSection(CurModule, CurOutput, (*Sect)->getSection());
          if (!ExpWrite) {
            Config.raiseDiagEntry(std::move(ExpWrite.error()));
            // FIXME: Change return type of this function from std::error_code
            // to eld::Expected<void>.
            // Return generic error-code. The actual error is already reported.
            return make_error_code(std::errc::not_supported);
          }
        }
      }
    }
  } else {
    eld::RegisterTimer T("Emit Regular ELF Sections", "Emit Output File",
                         Config.options().printTimingStats());
    // Write out regular ELF sections
    Module::iterator Sect, SectEnd = CurModule.end();
    for (Sect = CurModule.begin(); Sect != SectEnd; ++Sect) {
      OutputSectionEntry *E = (*Sect)->getOutputSection();
      for (auto &InputRule : *E) {
        eld::Expected<void> ExpWrite =
            writeSection(CurModule, CurOutput, InputRule->getSection());
        if (!ExpWrite) {
          Config.raiseDiagEntry(std::move(ExpWrite.error()));
          // FIXME: Change return type of this function from std::error_code
          // to eld::Expected<void>.
          // Return generic error-code. The actual error is already reported.
          return make_error_code(std::errc::not_supported);
        }
      }
    }
    for (Sect = CurModule.begin(); Sect != SectEnd; ++Sect) {
      eld::Expected<void> ExpWrite = writeSection(CurModule, CurOutput, *Sect);
      if (!ExpWrite) {
        Config.raiseDiagEntry(std::move(ExpWrite.error()));
        // FIXME: Change return type of this function from std::error_code
        // to eld::Expected<void>.
        // Return generic error-code. The actual error is already reported.
        return make_error_code(std::errc::not_supported);
      }
    }

    emitShStrTab(target().getOutputFormat()->getShStrTab(), CurModule,
                 CurOutput);

    if (Config.targets().is32Bits()) {
      // Write out ELF header
      // Write out section header table
      writeELFHeader<llvm::object::ELF32LE>(Config, CurModule, CurOutput);
      if (IsDynobj || IsExec)
        emitProgramHeader<llvm::object::ELF32LE>(CurOutput);

      emitSectionHeader<llvm::object::ELF32LE>(CurModule, Config, CurOutput);
    } else if (Config.targets().is64Bits()) {
      // Write out ELF header
      // Write out section header table
      writeELFHeader<llvm::object::ELF64LE>(Config, CurModule, CurOutput);
      if (IsDynobj || IsExec)
        emitProgramHeader<llvm::object::ELF64LE>(CurOutput);

      emitSectionHeader<llvm::object::ELF64LE>(CurModule, Config, CurOutput);
    }
  }

  return std::error_code();
}

// getOutputSize - count the final output size
template <typename ELFT>
size_t ELFObjectWriter::getOutputSize(const Module &CurModule) const {
  return getLastStartOffset<ELFT>(CurModule) +
         sizeof(typename ELFT::Shdr) * CurModule.size();
}

// writeELFHeader - emit ElfXX_Ehdr
template <typename ELFT>
void ELFObjectWriter::writeELFHeader(LinkerConfig &CurConfig,
                                     const Module &CurModule,
                                     llvm::FileOutputBuffer &CurOutput) const {
  typedef typename ELFT::Ehdr ElfXX_Ehdr;
  typedef typename ELFT::Phdr ElfXX_Phdr;
  typedef typename ELFT::Shdr ElfXX_Shdr;

  // ELF header must start from 0x0
  MemoryRegion Region =
      Backend.getFileOutputRegion(CurOutput, 0, sizeof(ElfXX_Ehdr));
  ElfXX_Ehdr *Header = (ElfXX_Ehdr *)Region.begin();

  memcpy(Header->e_ident, ElfMagic, EI_MAG3 + 1);

  Header->e_ident[EI_CLASS] = (!ELFT::Is64Bits) ? ELFCLASS32 : ELFCLASS64;
  Header->e_ident[EI_DATA] =
      CurConfig.targets().isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;
  Header->e_ident[EI_VERSION] = target().getInfo().ELFVersion();
  Header->e_ident[EI_OSABI] = target().getInfo().OSABI();
  Header->e_ident[EI_ABIVERSION] = target().getInfo().ABIVersion();

  // FIXME: add processor-specific and core file types.
  switch (CurConfig.codeGenType()) {
  case LinkerConfig::Object:
    Header->e_type = ET_REL;
    break;
  case LinkerConfig::DynObj:
    Header->e_type = ET_DYN;
    break;
  case LinkerConfig::Exec:
    Header->e_type = ET_EXEC;
    break;
  default:
    Config.raise(Diag::unsupported_output_file_type) << CurConfig.codeGenType();
    Header->e_type = ET_NONE;
  }
  Header->e_machine = target().getInfo().machine();
  Header->e_version = Header->e_ident[EI_VERSION];
  Header->e_entry = getEntryPoint(CurConfig, CurModule);

  if (LinkerConfig::Object != CurConfig.codeGenType())
    Header->e_phoff = sizeof(ElfXX_Ehdr);
  else
    Header->e_phoff = 0x0;

  Header->e_shoff = getLastStartOffset<ELFT>(CurModule);
  Header->e_flags = target().getInfo().flags();
  Header->e_ehsize = sizeof(ElfXX_Ehdr);
  Header->e_phentsize = sizeof(ElfXX_Phdr);
  Header->e_phnum = target().elfSegmentTable().size();
  Header->e_shentsize = sizeof(ElfXX_Shdr);
  if (CurModule.size() >= SHN_LORESERVE)
    Header->e_shnum = SHN_UNDEF;
  else
    Header->e_shnum = CurModule.size();
  size_t Shstrndx = CurModule.getSection(".shstrtab")->getIndex();
  if (Shstrndx >= SHN_LORESERVE)
    Header->e_shstrndx = SHN_XINDEX;
  else
    Header->e_shstrndx = Shstrndx;
}

/// getEntryPoint
uint64_t ELFObjectWriter::getEntryPoint(LinkerConfig &CurConfig,
                                        const Module &CurModule) const {
  llvm::StringRef EntryName = target().getEntry();
  uint64_t Result = 0x0;

  bool IssueWarning = (CurConfig.options().hasEntry() &&
                       LinkerConfig::Object != CurConfig.codeGenType() &&
                       LinkerConfig::DynObj != CurConfig.codeGenType());

  if (string::isValidCIdentifier(EntryName)) {
    const LDSymbol *EntrySymbol =
        CurModule.getNamePool().findSymbol(EntryName.str());
    if (EntrySymbol &&
        (EntrySymbol->desc() != ResolveInfo::Define && IssueWarning)) {
      Config.raise(Diag::warn_entry_symbol_not_found) << EntryName << 0;
      return (Result = 0);
    }
    if (!EntrySymbol)
      return (Result = 0);
    Result = EntrySymbol->value();
  } else {
    if (EntryName.getAsInteger(0, Result)) {
      Config.raise(Diag::warn_entry_symbol_bad_value) << EntryName << 0;
      return Result = 0;
    }
  }
  return Result;
}

// emitSectionHeader - emit ElfXX_Shdr
template <typename ELFT>
void ELFObjectWriter::emitSectionHeader(
    const Module &CurModule, LinkerConfig &CurConfig,
    llvm::FileOutputBuffer &CurOutput) const {
  typedef typename ELFT::Shdr ElfXX_Shdr;

  // emit section header
  unsigned int SectNum = CurModule.size();
  unsigned int HeaderSize = sizeof(ElfXX_Shdr) * SectNum;
  MemoryRegion Region = Backend.getFileOutputRegion(
      CurOutput, getLastStartOffset<ELFT>(CurModule), HeaderSize);
  ElfXX_Shdr *Shdr = (ElfXX_Shdr *)Region.begin();

  unsigned int SectIdx = 0;
  unsigned int Shstridx = 0; // nullptr section has empty name
  for (; SectIdx < SectNum; ++SectIdx) {
    ELFSection *LdSect = CurModule.getSectionTable().at(SectIdx);
    Shdr[SectIdx].sh_name = Shstridx;
    Shdr[SectIdx].sh_type = LdSect->getType();
    Shdr[SectIdx].sh_flags = LdSect->getFlags();
    Shdr[SectIdx].sh_addr = (LdSect->isAlloc()) ? LdSect->addr() : 0;
    Shdr[SectIdx].sh_offset = LdSect->offset();
    if (SectIdx == 0 && CurModule.size() >= SHN_LORESERVE)
      Shdr[SectIdx].sh_size = CurModule.size();
    else
      Shdr[SectIdx].sh_size = LdSect->size();
    Shdr[SectIdx].sh_addralign = LdSect->getAddrAlign();
    Shdr[SectIdx].sh_entsize = getSectEntrySize<ELFT>(LdSect);
    if (SectIdx == 0 &&
        CurModule.getSection(".shstrtab")->getIndex() >= SHN_LORESERVE)
      Shdr[SectIdx].sh_link = CurModule.getSection(".shstrtab")->getIndex();
    else
      Shdr[SectIdx].sh_link = getSectLink(LdSect);
    Shdr[SectIdx].sh_info = getSectInfo(LdSect);

    // adjust strshidx
    Shstridx += LdSect->name().size() + 1;
  }
}

// emitProgramHeader - emit ElfXX_Phdr
template <typename ELFT>
void ELFObjectWriter::emitProgramHeader(
    llvm::FileOutputBuffer &CurOutput) const {
  typedef typename ELFT::Ehdr ElfXX_Ehdr;
  typedef typename ELFT::Phdr ElfXX_Phdr;

  uint64_t StartOffset, PhdrSize;

  StartOffset = sizeof(ElfXX_Ehdr);
  PhdrSize = sizeof(ElfXX_Phdr);
  // Program header must start directly after ELF header
  MemoryRegion Region = Backend.getFileOutputRegion(
      CurOutput, StartOffset, target().elfSegmentTable().size() * PhdrSize);

  ElfXX_Phdr *Phdr = (ElfXX_Phdr *)Region.begin();

  // Iterate the elf segment table in GNULDBackend
  size_t Index = 0;
  auto Seg = target().elfSegmentTable().begin(),
       SegEnd = target().elfSegmentTable().end();
  for (; Seg != SegEnd; ++Seg) {
    Phdr[Index].p_type = (*Seg)->type();
    Phdr[Index].p_flags = (*Seg)->flag();
    Phdr[Index].p_offset = (*Seg)->offset();
    Phdr[Index].p_vaddr = (*Seg)->vaddr();
    Phdr[Index].p_paddr = (*Seg)->paddr();
    Phdr[Index].p_filesz = (*Seg)->filesz();
    Phdr[Index].p_memsz = (*Seg)->memsz();
    Phdr[Index].p_align = (*Seg)->align();
    ++Index;
  }
}

/// emitShStrTab - emit section string table
void ELFObjectWriter::emitShStrTab(ELFSection *PShStrTab,
                                   const Module &CurModule,
                                   llvm::FileOutputBuffer &CurOutput) {
  // write out data
  MemoryRegion Region = Backend.getFileOutputRegion(
      CurOutput, PShStrTab->offset(), PShStrTab->size());
  char *Data = (char *)Region.begin();
  size_t Shstrsize = 0;
  Module::const_iterator Section, SectEnd = CurModule.end();
  for (Section = CurModule.begin(); Section != SectEnd; ++Section) {
    strcpy((char *)(Data + Shstrsize), (*Section)->name().data());
    Shstrsize += (*Section)->name().size() + 1;
  }
}

/// emitRelocation
template <typename ELFT>
void ELFObjectWriter::emitRelocation(Module &Module, ELFSection *CurSection,
                                     MemoryRegion &CurRegion,
                                     bool isDyn) const {
  if (CurSection->getType() == SHT_REL) {
    emitRel<ELFT>(Module, CurSection, CurRegion, isDyn);
  }
  if (CurSection->getType() == SHT_RELA) {
    emitRela<ELFT>(Module, CurSection, CurRegion, isDyn);
  }
}

// emitRel - emit ElfXX_Rel
template <typename ELFT>
void ELFObjectWriter::emitRel(Module &Module, ELFSection *CurSection,
                              MemoryRegion &CurRegion, bool isDyn) const {
  typedef typename ELFT::Rel ElfXX_Rel;
  typedef typename ELFT::Addr ElfXX_Addr;
  typedef typename ELFT::Word ElfXX_Word;

  ElfXX_Rel *Rel = reinterpret_cast<ElfXX_Rel *>(CurRegion.begin());

  FragmentRef *FragRef = nullptr;

  for (auto &R : CurSection->getRelocations()) {
    ElfXX_Addr ROffset = (ElfXX_Addr)0;
    ElfXX_Word RSym = (ElfXX_Word)0;
    FragRef = R->targetRef();

    if ((isDyn || !Config.options().emitRelocs() ||
         Config.options().emitGNUCompatRelocs()) &&
        (LinkerConfig::DynObj == Config.codeGenType() ||
         LinkerConfig::Exec == Config.codeGenType())) {
      ROffset = static_cast<ElfXX_Addr>(FragRef->getOutputELFSection()->addr() +
                                        FragRef->getOutputOffset(Module));
    } else {
      ROffset = static_cast<ElfXX_Addr>(FragRef->getOutputOffset(Module));
    }

    if (!isDyn) {
      if (R->symInfo()) {
        RSym = static_cast<ElfXX_Word>(
            target().getSymbolIdx(R->symInfo()->outSymbol(), true));
        if (shouldEmitReloc(R)) {
          emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset);
          ++Rel;
        }
      }
      continue;
    }

    GNULDBackend::DynRelocType DynRelocType = target().getDynRelocType(R);

    switch (DynRelocType) {
    case GNULDBackend::DynRelocType::DEFAULT:
    case GNULDBackend::DynRelocType::JMP_SLOT:
    case GNULDBackend::DynRelocType::GLOB_DAT:
    case GNULDBackend::DynRelocType::TPREL_GLOBAL:
    case GNULDBackend::DynRelocType::WORD_DEPOSIT: {
      RSym = static_cast<ElfXX_Word>(
          target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset);
      break;
    }
    case GNULDBackend::DynRelocType::RELATIVE:
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset);
      break;
    case GNULDBackend::DynRelocType::TLSDESC_GLOBAL:
    case GNULDBackend::DynRelocType::TLSDESC_LOCAL:
      break;
    case GNULDBackend::DynRelocType::DTPMOD_GLOBAL:
    case GNULDBackend::DynRelocType::DTPREL_GLOBAL: {
      if (R->symInfo())
        RSym = static_cast<ElfXX_Word>(
            target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset);
      break;
    }
    case GNULDBackend::DynRelocType::DTPMOD_LOCAL:
    case GNULDBackend::DynRelocType::DTPREL_LOCAL:
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset);
      break;

    case GNULDBackend::DynRelocType::TPREL_LOCAL:
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset);
      break;
    }
    ++Rel;
  }
}

// emitRela - emit ElfXX_Rela
template <typename ELFT>
void ELFObjectWriter::emitRela(Module &Module, ELFSection *S,
                               MemoryRegion &CurRegion, bool isDyn) const {
  typedef typename ELFT::Rela ElfXX_Rela;
  typedef typename ELFT::Addr ElfXX_Addr;
  typedef typename ELFT::Word ElfXX_Word;

  ElfXX_Rela *Rel = reinterpret_cast<ElfXX_Rela *>(CurRegion.begin());

  FragmentRef *FragRef = nullptr;

  for (auto &R : S->getRelocations()) {
    ElfXX_Addr ROffset = (ElfXX_Addr)0;
    ElfXX_Word RSym = (ElfXX_Word)0;

    FragRef = R->targetRef();

    if ((isDyn || !Config.options().emitRelocs() ||
         Config.options().emitGNUCompatRelocs()) &&
        (LinkerConfig::DynObj == Config.codeGenType() ||
         LinkerConfig::Exec == Config.codeGenType())) {
      ROffset = static_cast<ElfXX_Addr>(FragRef->getOutputELFSection()->addr() +
                                        FragRef->getOutputOffset(Module));
    } else {
      ROffset = static_cast<ElfXX_Addr>(FragRef->getOutputOffset(Module));
    }

    auto RAddend = R->addend();
    if (!isDyn) {
      if (R->symInfo()) {
        // TODO: QTOOL-102747: relocations to discarded debug sections
        // should be removed when tombstones are inserted or replaced with
        // REL_NONE. In that case getSymbolIdx should never be expected to
        // look for an invalid symbol.
        RSym = static_cast<ElfXX_Word>(
            target().getSymbolIdx(R->symInfo()->outSymbol(), true));
        if (shouldEmitReloc(R)) {
          emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset, RAddend);
          ++Rel;
        }
      }
      continue;
    }

    GNULDBackend::DynRelocType DynRelocType = target().getDynRelocType(R);

    switch (DynRelocType) {
    case GNULDBackend::DynRelocType::DEFAULT:
    case GNULDBackend::DynRelocType::JMP_SLOT:
    case GNULDBackend::DynRelocType::GLOB_DAT:
    case GNULDBackend::DynRelocType::TPREL_GLOBAL:
    case GNULDBackend::DynRelocType::WORD_DEPOSIT: {
      RSym = static_cast<ElfXX_Word>(
          target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset, RAddend);
      break;
    }
    case GNULDBackend::DynRelocType::RELATIVE: {
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset,
                           target().getRelocator()->getSymValue(R) + RAddend);
      // llvm::errs() << "Writer Name : " << R->symInfo()->name() << " Addend :
      // " << r_addend << "\n";
    } break;

    case GNULDBackend::DynRelocType::TLSDESC_GLOBAL:
    case GNULDBackend::DynRelocType::DTPMOD_GLOBAL:
    case GNULDBackend::DynRelocType::DTPREL_GLOBAL: {
      if (R->symInfo())
        RSym = static_cast<ElfXX_Word>(
            target().getDynSymbolIdx(R->symInfo()->outSymbol()));
      emitRelocation<ELFT>(*Rel, R->type(), RSym, ROffset, RAddend);
      break;
    }
    case GNULDBackend::DynRelocType::TLSDESC_LOCAL:
    case GNULDBackend::DynRelocType::DTPMOD_LOCAL:
    case GNULDBackend::DynRelocType::DTPREL_LOCAL:
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset, 0);
      break;

    case GNULDBackend::DynRelocType::TPREL_LOCAL:
      emitRelocation<ELFT>(*Rel, R->type(), 0, ROffset,
                           target().getRelocator()->getSymValue(R) + RAddend);
      break;
    }
    ++Rel;
  }
}

/// getSectEntrySize - compute ElfXX_Shdr::sh_entsize
template <typename ELFT>
uint64_t ELFObjectWriter::getSectEntrySize(ELFSection *CurSection) const {
  typedef typename ELFT::Word ElfXX_Word;
  typedef typename ELFT::Sym ElfXX_Sym;
  typedef typename ELFT::Rel ElfXX_Rel;
  typedef typename ELFT::Rela ElfXX_Rela;
  typedef typename ELFT::Dyn ElfXX_Dyn;

  if (CurSection->isGroupKind())
    return sizeof(llvm::ELF::Elf32_Word);
  if (llvm::ELF::SHT_DYNSYM == CurSection->getType() ||
      llvm::ELF::SHT_SYMTAB == CurSection->getType())
    return sizeof(ElfXX_Sym);
  if (CurSection->isRel())
    return sizeof(ElfXX_Rel);
  if (CurSection->isRela())
    return sizeof(ElfXX_Rela);
  if (llvm::ELF::SHT_HASH == CurSection->getType() ||
      llvm::ELF::SHT_GNU_HASH == CurSection->getType() ||
      llvm::ELF::SHT_SYMTAB_SHNDX == CurSection->getType())
    return sizeof(ElfXX_Word);
  if (llvm::ELF::SHT_DYNAMIC == CurSection->getType())
    return sizeof(ElfXX_Dyn);
  // FIXME: We should get the entsize from input since the size of each
  // character is specified in the section header's sh_entsize field.
  // For example, traditional string is 0x1, UCS-2 is 0x2, ... and so on.
  // Ref: http://www.sco.com/developers/gabi/2003-12-17/ch4.sheader.html
  if (CurSection->getFlags() & llvm::ELF::SHF_STRINGS)
    return CurSection->getEntSize();
  return 0x0;
}

/// getSectLink - compute ElfXX_Shdr::sh_link
uint64_t ELFObjectWriter::getSectLink(const ELFSection *S) const {
  ELFSection *Link = nullptr;
  if (S->isGroupKind())
    Link = target().getOutputFormat()->getSymTab();
  if (llvm::ELF::SHT_SYMTAB == S->getType())
    Link = target().getOutputFormat()->getStrTab();
  if (llvm::ELF::SHT_SYMTAB_SHNDX == S->getType())
    Link = target().getOutputFormat()->getSymTab();
  if (llvm::ELF::SHT_DYNSYM == S->getType())
    Link = target().getOutputFormat()->getDynStrTab();
  if (llvm::ELF::SHT_DYNAMIC == S->getType())
    Link = target().getOutputFormat()->getDynStrTab();
  if (llvm::ELF::SHT_HASH == S->getType() ||
      llvm::ELF::SHT_GNU_HASH == S->getType())
    Link = target().getOutputFormat()->getDynSymTab();
  if (Config.isLinkPartial() && llvm::ELF::SHF_LINK_ORDER & S->getFlags())
    return S->getLink()->getOutputSection()->getSection()->getIndex();
  if (S->isRelocationSection()) {
    if (S->getKind() != LDFileFormat::DynamicRelocation)
      Link = target().getOutputFormat()->getSymTab();
    else
      Link = target().getOutputFormat()->getDynSymTab();
  }
  if (!Link)
    return Backend.getSectLink(S);

  if (Link->isIgnore() || Link->isDiscard())
    return 0;
  return Link->getIndex();
}

/// getSectInfo - compute ElfXX_Shdr::sh_info
uint64_t ELFObjectWriter::getSectInfo(ELFSection *CurSection) const {
  if (CurSection->isGroupKind())
    return Backend.getSymbolIdx(CurSection->getSymbol());

  if (llvm::ELF::SHT_SYMTAB == CurSection->getType() ||
      llvm::ELF::SHT_DYNSYM == CurSection->getType())
    return CurSection->getInfo();

  if (CurSection->isRelocationSection()) {
    auto *InfoLink = llvm::dyn_cast_or_null<ELFSection>(CurSection->getLink());
    if (nullptr != InfoLink) {
      uint32_t Idx = Backend.getSectionIdx(InfoLink);
      if (Idx != UINT32_MAX)
        return Idx;
      Idx = Backend.getSectionIdx(InfoLink->getOutputELFSection());
      ASSERT(Idx != UINT32_MAX, "Relocation section " +
                                    CurSection->name().str() + "is not linked");
      if (Idx != UINT32_MAX)
        return Idx;
    }
  }

  return 0x0;
}

/// getLastStartOffset
template <typename ELFT>
uint64_t ELFObjectWriter::getLastStartOffset(const Module &CurModule) const {
  const ELFSection *lastSect = CurModule.back();
  assert(lastSect != nullptr);
  if (ELFT::Is64Bits)
    return llvm::alignTo<64>(lastSect->offset() + lastSect->size());
  return llvm::alignTo<32>(lastSect->offset() + lastSect->size());
}

/// emitGroupData
void ELFObjectWriter::emitGroup(ELFSection *S, MemoryRegion &CurRegion) {
  size_t CurOffset = 0;
  for (auto &Frag : S->getFragmentList()) {
    if (Frag->getKind() != Fragment::Region)
      continue;
    RegionFragment &RegionFrag = llvm::cast<RegionFragment>(*Frag);
    const uint8_t *From = (const uint8_t *)RegionFrag.getRegion().data();
    size_t GroupDataSize =
        RegionFrag.getRegion().size() / sizeof(llvm::ELF::Elf32_Word);
    // copy the contents
    memcpy(CurRegion.begin() + CurOffset, From, Frag->size());
    void *GroupData = (void *)(CurRegion.begin() + CurOffset);
    auto *Si = S->getGroupSections().begin();
    auto *Se = S->getGroupSections().end();
    for (size_t Index = 1; Index < GroupDataSize; ++Index) {
      if (Si == Se)
        break;
      uint32_t SectionIdx = (*Si)->getOutputELFSection()->getIndex();
      ((uint32_t *)GroupData)[Index] = SectionIdx;
      ++Si;
    }
    CurOffset += Frag->size();
  }
}

eld::Expected<void>
ELFObjectWriter::emitSection(ELFSection *S, MemoryRegion &CurRegion) const {
  for (auto &Frag : S->getFragmentList()) {
    eld::Expected<void> ExpEmit = Frag->emit(CurRegion, Backend.getModule());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpEmit);
  }
  return {};
}

bool ELFObjectWriter::shouldEmitReloc(const Relocation *R) const {
  ResolveInfo *RI = R->symInfo();
  if (RI == ResolveInfo::null())
    return true;
  return target().getSymbolIdx(RI->outSymbol(), /*IgnoreUnknown=*/true);
}

/// emitRelocation - write data to the ELF32_Rel entry
template <typename ELFT>
void ELFObjectWriter::emitRelocation(typename ELFT::Rel &pRel,
                                     Relocation::Type pType, uint32_t pSymIdx,
                                     uint32_t pOffset) const {
  pRel.r_offset = pOffset;
  pRel.setSymbolAndType(pSymIdx, pType, false);
}

/// emitRelocation - write data to the ELF32_Rela entry
template <typename ELFT>
void ELFObjectWriter::emitRelocation(typename ELFT::Rela &pRel,
                                     Relocation::Type pType, uint32_t pSymIdx,
                                     uint32_t pOffset, int32_t pAddend) const {
  pRel.r_offset = pOffset;
  pRel.r_addend = pAddend;
  pRel.setSymbolAndType(pSymIdx, pType, false);
}

template size_t ELFObjectWriter::getOutputSize<llvm::object::ELF32LE>(
    const Module &CurModule) const;
template size_t ELFObjectWriter::getOutputSize<llvm::object::ELF64LE>(
    const Module &CurModule) const;
template void ELFObjectWriter::writeELFHeader<llvm::object::ELF32LE>(
    LinkerConfig &CurConfig, const Module &CurModule,
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::writeELFHeader<llvm::object::ELF64LE>(
    LinkerConfig &CurConfig, const Module &CurModule,
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::emitSectionHeader<llvm::object::ELF32LE>(
    const Module &CurModule, LinkerConfig &CurConfig,
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::emitSectionHeader<llvm::object::ELF64LE>(
    const Module &CurModule, LinkerConfig &CurConfig,
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::emitProgramHeader<llvm::object::ELF32LE>(
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::emitProgramHeader<llvm::object::ELF64LE>(
    llvm::FileOutputBuffer &CurOutput) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF32LE>(
    Module &Module, ELFSection *CurSection, MemoryRegion &CurRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF64LE>(
    Module &Module, ELFSection *CurSection, MemoryRegion &CurRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRel<llvm::object::ELF32LE>(
    Module &Module, ELFSection *CurSection, MemoryRegion &CurRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRela<llvm::object::ELF64LE>(
    Module &Module, ELFSection *CurSection, MemoryRegion &CurRegion,
    bool isDyn) const;
template uint64_t ELFObjectWriter::getSectEntrySize<llvm::object::ELF32LE>(
    ELFSection *CurSection) const;
template uint64_t ELFObjectWriter::getSectEntrySize<llvm::object::ELF64LE>(
    ELFSection *CurSection) const;
template uint64_t ELFObjectWriter::getLastStartOffset<llvm::object::ELF32LE>(
    const Module &CurModule) const;
template uint64_t ELFObjectWriter::getLastStartOffset<llvm::object::ELF64LE>(
    const Module &CurModule) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF32LE>(
    llvm::object::ELF32LE::Rel &pRel, Relocation::Type pType, uint32_t pSymIdx,
    uint32_t pOffset) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF64LE>(
    llvm::object::ELF64LE::Rel &pRel, Relocation::Type pType, uint32_t pSymIdx,
    uint32_t pOffset) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF32LE>(
    llvm::object::ELF32LE::Rela &pRela, Relocation::Type pType,
    uint32_t pSymIdx, uint32_t pOffset, int32_t Addend) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF64LE>(
    llvm::object::ELF64LE::Rela &pRela, Relocation::Type pType,
    uint32_t pSymIdx, uint32_t pOffset, int32_t Addend) const;
