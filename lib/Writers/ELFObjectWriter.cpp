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
ELFObjectWriter::ELFObjectWriter(GNULDBackend &pBackend, LinkerConfig &pConfig)
    : m_Backend(pBackend), m_Config(pConfig) {}

ELFObjectWriter::~ELFObjectWriter() {}

eld::Expected<void>
ELFObjectWriter::writeSection(Module &pModule, llvm::FileOutputBuffer &pOutput,
                              ELFSection *section) {
  MemoryRegion region;
  // Request output region
  switch (section->getKind()) {
  case LDFileFormat::Note:
    if (!section->hasSectionData())
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
    if (section->getOutputELFSection()->isNoBits())
      return {};
    region = m_Backend.getFileOutputRegion(
        pOutput, section->getOutputELFSection()->offset(),
        section->getOutputELFSection()->size());
    if (region.size() == 0) {
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
    m_Config.raise(diag::unsupported_section_kind)
        << section->getKind() << section->name();
    return {};
  }
  eld::Expected<void> expWrite = writeRegion(pModule, section, region);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expWrite);
  return {};
}

eld::Expected<void> ELFObjectWriter::writeRegion(Module &pModule,
                                                 ELFSection *section,
                                                 MemoryRegion &region) {
  if (section->isNoBits())
    return {};
  if (section->isIgnore())
    return {};
  if (section->isDiscard())
    return {};
  // Write out sections with data
  switch (section->getKind()) {
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
    eld::Expected<void> expEmit = emitSection(section, region);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expEmit);
    break;
  }
  case LDFileFormat::DynamicRelocation: {
    // sort relocation for the benefit of the dynamic linker.
    target().sortRelocation(*section);
    if (m_Config.targets().is32Bits())
      emitRelocation<llvm::object::ELF32LE>(pModule, section, region, true);
    if (m_Config.targets().is64Bits())
      emitRelocation<llvm::object::ELF64LE>(pModule, section, region, true);
    break;
  }
  case LDFileFormat::Relocation: {
    if (m_Config.targets().is32Bits())
      emitRelocation<llvm::object::ELF32LE>(pModule, section, region, false);
    if (m_Config.targets().is64Bits())
      emitRelocation<llvm::object::ELF64LE>(pModule, section, region, false);
    break;
  }
  case LDFileFormat::Target: {
    auto expEmit = target().emitSection(section, region);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expEmit);
  } break;
  case LDFileFormat::Group:
    emitGroup(section, region);
    break;
  default:
    llvm_unreachable("invalid section kind");
  }
  return {};
}

void ELFObjectWriter::writeLinkTimeStats(Module &pModule,
                                         uint64_t beginningOfTime,
                                         uint64_t duration) {

  TimingFragment *F = pModule.getBackend()->getTimingFragment();
  F->setData(beginningOfTime, duration);
}

std::error_code ELFObjectWriter::writeObject(Module &pModule,
                                             llvm::FileOutputBuffer &pOutput) {
  bool is_dynobj = m_Config.codeGenType() == LinkerConfig::DynObj;
  bool is_exec = m_Config.codeGenType() == LinkerConfig::Exec;
  bool is_binary = m_Config.codeGenType() == LinkerConfig::Binary;
  bool is_object = m_Config.codeGenType() == LinkerConfig::Object;

  assert(is_dynobj || is_exec || is_binary || is_object);

  if (is_dynobj || is_exec) {
    // Write out name pool sections: .dynsym, .dynstr, .hash
    eld::RegisterTimer T("Emit Dynamic Name Pool sections", "Emit Output File",
                         m_Config.options().printTimingStats());
    if (!target().emitDynNamePools(pOutput))
      return make_error_code(std::errc::function_not_supported);
  }

  if (is_object || is_dynobj || is_exec) {
    // Write out name pool sections: .symtab, .strtab
    auto E = target().emitRegNamePools(pOutput);
    if (!E) {
      m_Config.getDiagEngine()->raiseDiagEntry(std::move(E.error()));
      return make_error_code(std::errc::no_such_file_or_directory);
    }
  }

  {
    PluginManager &PM = pModule.getPluginManager();
    if (!PM.callActBeforeWritingOutputHook()) {
      // Return generic error-code. Actual error is already reported!
      return make_error_code(std::errc::not_supported);
    }
  }

  if (is_binary) {
    // Iterate over the loadable segments and write the corresponding sections
    ELFSegmentFactory::iterator seg, segEnd = target().elfSegmentTable().end();

    for (seg = target().elfSegmentTable().begin(); seg != segEnd; ++seg) {
      if (llvm::ELF::PT_LOAD == (*seg)->type()) {
        ELFSegment::iterator sect, sectEnd = (*seg)->end();
        for (sect = (*seg)->begin(); sect != sectEnd; ++sect) {
          eld::Expected<void> expWrite =
              writeSection(pModule, pOutput, (*sect)->getSection());
          if (!expWrite) {
            m_Config.raiseDiagEntry(std::move(expWrite.error()));
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
                         m_Config.options().printTimingStats());
    // Write out regular ELF sections
    Module::iterator sect, sectEnd = pModule.end();
    for (sect = pModule.begin(); sect != sectEnd; ++sect) {
      OutputSectionEntry *E = (*sect)->getOutputSection();
      for (auto &InputRule : *E) {
        eld::Expected<void> expWrite =
            writeSection(pModule, pOutput, InputRule->getSection());
        if (!expWrite) {
          m_Config.raiseDiagEntry(std::move(expWrite.error()));
          // FIXME: Change return type of this function from std::error_code
          // to eld::Expected<void>.
          // Return generic error-code. The actual error is already reported.
          return make_error_code(std::errc::not_supported);
        }
      }
    }
    for (sect = pModule.begin(); sect != sectEnd; ++sect) {
      eld::Expected<void> expWrite = writeSection(pModule, pOutput, *sect);
      if (!expWrite) {
        m_Config.raiseDiagEntry(std::move(expWrite.error()));
        // FIXME: Change return type of this function from std::error_code
        // to eld::Expected<void>.
        // Return generic error-code. The actual error is already reported.
        return make_error_code(std::errc::not_supported);
      }
    }

    emitShStrTab(target().getOutputFormat()->getShStrTab(), pModule, pOutput);

    if (m_Config.targets().is32Bits()) {
      // Write out ELF header
      // Write out section header table
      writeELFHeader<llvm::object::ELF32LE>(m_Config, pModule, pOutput);
      if (is_dynobj || is_exec)
        emitProgramHeader<llvm::object::ELF32LE>(pOutput);

      emitSectionHeader<llvm::object::ELF32LE>(pModule, m_Config, pOutput);
    } else if (m_Config.targets().is64Bits()) {
      // Write out ELF header
      // Write out section header table
      writeELFHeader<llvm::object::ELF64LE>(m_Config, pModule, pOutput);
      if (is_dynobj || is_exec)
        emitProgramHeader<llvm::object::ELF64LE>(pOutput);

      emitSectionHeader<llvm::object::ELF64LE>(pModule, m_Config, pOutput);
    }
  }

  return std::error_code();
}

// getOutputSize - count the final output size
template <typename ELFT>
size_t ELFObjectWriter::getOutputSize(const Module &pModule) const {
  return getLastStartOffset<ELFT>(pModule) +
         sizeof(typename ELFT::Shdr) * pModule.size();
}

// writeELFHeader - emit ElfXX_Ehdr
template <typename ELFT>
void ELFObjectWriter::writeELFHeader(LinkerConfig &pConfig,
                                     const Module &pModule,
                                     llvm::FileOutputBuffer &pOutput) const {
  typedef typename ELFT::Ehdr ElfXX_Ehdr;
  typedef typename ELFT::Phdr ElfXX_Phdr;
  typedef typename ELFT::Shdr ElfXX_Shdr;

  // ELF header must start from 0x0
  MemoryRegion region =
      m_Backend.getFileOutputRegion(pOutput, 0, sizeof(ElfXX_Ehdr));
  ElfXX_Ehdr *header = (ElfXX_Ehdr *)region.begin();

  memcpy(header->e_ident, ElfMagic, EI_MAG3 + 1);

  header->e_ident[EI_CLASS] = (!ELFT::Is64Bits) ? ELFCLASS32 : ELFCLASS64;
  header->e_ident[EI_DATA] =
      pConfig.targets().isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;
  header->e_ident[EI_VERSION] = target().getInfo().ELFVersion();
  header->e_ident[EI_OSABI] = target().getInfo().OSABI();
  header->e_ident[EI_ABIVERSION] = target().getInfo().ABIVersion();

  // FIXME: add processor-specific and core file types.
  switch (pConfig.codeGenType()) {
  case LinkerConfig::Object:
    header->e_type = ET_REL;
    break;
  case LinkerConfig::DynObj:
    header->e_type = ET_DYN;
    break;
  case LinkerConfig::Exec:
    header->e_type = ET_EXEC;
    break;
  default:
    m_Config.raise(diag::unsupported_output_file_type) << pConfig.codeGenType();
    header->e_type = ET_NONE;
  }
  header->e_machine = target().getInfo().machine();
  header->e_version = header->e_ident[EI_VERSION];
  header->e_entry = getEntryPoint(pConfig, pModule);

  if (LinkerConfig::Object != pConfig.codeGenType())
    header->e_phoff = sizeof(ElfXX_Ehdr);
  else
    header->e_phoff = 0x0;

  header->e_shoff = getLastStartOffset<ELFT>(pModule);
  header->e_flags = target().getInfo().flags();
  header->e_ehsize = sizeof(ElfXX_Ehdr);
  header->e_phentsize = sizeof(ElfXX_Phdr);
  header->e_phnum = target().elfSegmentTable().size();
  header->e_shentsize = sizeof(ElfXX_Shdr);
  if (pModule.size() >= SHN_LORESERVE)
    header->e_shnum = SHN_UNDEF;
  else
    header->e_shnum = pModule.size();
  size_t shstrndx = pModule.getSection(".shstrtab")->getIndex();
  if (shstrndx >= SHN_LORESERVE)
    header->e_shstrndx = SHN_XINDEX;
  else
    header->e_shstrndx = shstrndx;
}

/// getEntryPoint
uint64_t ELFObjectWriter::getEntryPoint(LinkerConfig &pConfig,
                                        const Module &pModule) const {
  llvm::StringRef entry_name = target().getEntry();
  uint64_t result = 0x0;

  bool issue_warning = (pConfig.options().hasEntry() &&
                        LinkerConfig::Object != pConfig.codeGenType() &&
                        LinkerConfig::DynObj != pConfig.codeGenType());

  if (string::isValidCIdentifier(entry_name)) {
    const LDSymbol *entry_symbol =
        pModule.getNamePool().findSymbol(entry_name.str());
    if (entry_symbol &&
        (entry_symbol->desc() != ResolveInfo::Define && issue_warning)) {
      m_Config.raise(diag::warn_entry_symbol_not_found) << entry_name << 0;
      return (result = 0);
    }
    if (!entry_symbol)
      return (result = 0);
    result = entry_symbol->value();
  } else {
    if (entry_name.getAsInteger(0, result)) {
      m_Config.raise(diag::warn_entry_symbol_bad_value) << entry_name << 0;
      return result = 0;
    }
  }
  return result;
}

// emitSectionHeader - emit ElfXX_Shdr
template <typename ELFT>
void ELFObjectWriter::emitSectionHeader(const Module &pModule,
                                        LinkerConfig &pConfig,
                                        llvm::FileOutputBuffer &pOutput) const {
  typedef typename ELFT::Shdr ElfXX_Shdr;

  // emit section header
  unsigned int sectNum = pModule.size();
  unsigned int header_size = sizeof(ElfXX_Shdr) * sectNum;
  MemoryRegion region = m_Backend.getFileOutputRegion(
      pOutput, getLastStartOffset<ELFT>(pModule), header_size);
  ElfXX_Shdr *shdr = (ElfXX_Shdr *)region.begin();

  unsigned int sectIdx = 0;
  unsigned int shstridx = 0; // nullptr section has empty name
  for (; sectIdx < sectNum; ++sectIdx) {
    ELFSection *ld_sect = pModule.getSectionTable().at(sectIdx);
    shdr[sectIdx].sh_name = shstridx;
    shdr[sectIdx].sh_type = ld_sect->getType();
    shdr[sectIdx].sh_flags = ld_sect->getFlags();
    shdr[sectIdx].sh_addr = (ld_sect->isAlloc()) ? ld_sect->addr() : 0;
    shdr[sectIdx].sh_offset = ld_sect->offset();
    if (sectIdx == 0 && pModule.size() >= SHN_LORESERVE)
      shdr[sectIdx].sh_size = pModule.size();
    else
      shdr[sectIdx].sh_size = ld_sect->size();
    shdr[sectIdx].sh_addralign = ld_sect->getAddrAlign();
    shdr[sectIdx].sh_entsize = getSectEntrySize<ELFT>(ld_sect);
    if (sectIdx == 0 &&
        pModule.getSection(".shstrtab")->getIndex() >= SHN_LORESERVE)
      shdr[sectIdx].sh_link = pModule.getSection(".shstrtab")->getIndex();
    else
      shdr[sectIdx].sh_link = getSectLink(ld_sect);
    shdr[sectIdx].sh_info = getSectInfo(ld_sect);

    // adjust strshidx
    shstridx += ld_sect->name().size() + 1;
  }
}

// emitProgramHeader - emit ElfXX_Phdr
template <typename ELFT>
void ELFObjectWriter::emitProgramHeader(llvm::FileOutputBuffer &pOutput) const {
  typedef typename ELFT::Ehdr ElfXX_Ehdr;
  typedef typename ELFT::Phdr ElfXX_Phdr;

  uint64_t start_offset, phdr_size;

  start_offset = sizeof(ElfXX_Ehdr);
  phdr_size = sizeof(ElfXX_Phdr);
  // Program header must start directly after ELF header
  MemoryRegion region = m_Backend.getFileOutputRegion(
      pOutput, start_offset, target().elfSegmentTable().size() * phdr_size);

  ElfXX_Phdr *phdr = (ElfXX_Phdr *)region.begin();

  // Iterate the elf segment table in GNULDBackend
  size_t index = 0;
  auto seg = target().elfSegmentTable().begin(),
       segEnd = target().elfSegmentTable().end();
  for (; seg != segEnd; ++seg) {
    phdr[index].p_type = (*seg)->type();
    phdr[index].p_flags = (*seg)->flag();
    phdr[index].p_offset = (*seg)->offset();
    phdr[index].p_vaddr = (*seg)->vaddr();
    phdr[index].p_paddr = (*seg)->paddr();
    phdr[index].p_filesz = (*seg)->filesz();
    phdr[index].p_memsz = (*seg)->memsz();
    phdr[index].p_align = (*seg)->align();
    ++index;
  }
}

/// emitShStrTab - emit section string table
void ELFObjectWriter::emitShStrTab(ELFSection *pShStrTab, const Module &pModule,
                                   llvm::FileOutputBuffer &pOutput) {
  // write out data
  MemoryRegion region = m_Backend.getFileOutputRegion(
      pOutput, pShStrTab->offset(), pShStrTab->size());
  char *data = (char *)region.begin();
  size_t shstrsize = 0;
  Module::const_iterator section, sectEnd = pModule.end();
  for (section = pModule.begin(); section != sectEnd; ++section) {
    strcpy((char *)(data + shstrsize), (*section)->name().data());
    shstrsize += (*section)->name().size() + 1;
  }
}

/// emitRelocation
template <typename ELFT>
void ELFObjectWriter::emitRelocation(Module &Module, ELFSection *pSection,
                                     MemoryRegion &pRegion, bool isDyn) const {
  if (pSection->getType() == SHT_REL) {
    emitRel<ELFT>(Module, pSection, pRegion, isDyn);
  }
  if (pSection->getType() == SHT_RELA) {
    emitRela<ELFT>(Module, pSection, pRegion, isDyn);
  }
}

// emitRel - emit ElfXX_Rel
template <typename ELFT>
void ELFObjectWriter::emitRel(Module &Module, ELFSection *pSection,
                              MemoryRegion &pRegion, bool isDyn) const {
  typedef typename ELFT::Rel ElfXX_Rel;
  typedef typename ELFT::Addr ElfXX_Addr;
  typedef typename ELFT::Word ElfXX_Word;

  ElfXX_Rel *rel = reinterpret_cast<ElfXX_Rel *>(pRegion.begin());

  FragmentRef *frag_ref = nullptr;

  for (auto &R : pSection->getRelocations()) {
    ElfXX_Addr r_offset = (ElfXX_Addr)0;
    ElfXX_Word r_sym = (ElfXX_Word)0;
    frag_ref = R->targetRef();

    if ((isDyn || !m_Config.options().emitRelocs() ||
         m_Config.options().emitGNUCompatRelocs()) &&
        (LinkerConfig::DynObj == m_Config.codeGenType() ||
         LinkerConfig::Exec == m_Config.codeGenType())) {
      r_offset =
          static_cast<ElfXX_Addr>(frag_ref->getOutputELFSection()->addr() +
                                  frag_ref->getOutputOffset(Module));
    } else {
      r_offset = static_cast<ElfXX_Addr>(frag_ref->getOutputOffset(Module));
    }

    if (!isDyn) {
      if (R->symInfo()) {
        r_sym = static_cast<ElfXX_Word>(
            target().getSymbolIdx(R->symInfo()->outSymbol(), true));
        if (shouldEmitReloc(R)) {
          emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset);
          ++rel;
        }
      }
      continue;
    }

    GNULDBackend::DynRelocType dynRelocType = target().getDynRelocType(R);

    switch (dynRelocType) {
    case GNULDBackend::DynRelocType::DEFAULT:
    case GNULDBackend::DynRelocType::JMP_SLOT:
    case GNULDBackend::DynRelocType::GLOB_DAT:
    case GNULDBackend::DynRelocType::TPREL_GLOBAL:
    case GNULDBackend::DynRelocType::WORD_DEPOSIT: {
      r_sym = static_cast<ElfXX_Word>(
          target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset);
      break;
    }
    case GNULDBackend::DynRelocType::RELATIVE:
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset);
      break;
    case GNULDBackend::DynRelocType::TLSDESC_GLOBAL:
    case GNULDBackend::DynRelocType::TLSDESC_LOCAL:
      break;
    case GNULDBackend::DynRelocType::DTPMOD_GLOBAL:
    case GNULDBackend::DynRelocType::DTPREL_GLOBAL: {
      if (R->symInfo())
        r_sym = static_cast<ElfXX_Word>(
            target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset);
      break;
    }
    case GNULDBackend::DynRelocType::DTPMOD_LOCAL:
    case GNULDBackend::DynRelocType::DTPREL_LOCAL:
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset);
      break;

    case GNULDBackend::DynRelocType::TPREL_LOCAL:
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset);
      break;
    }
    ++rel;
  }
}

// emitRela - emit ElfXX_Rela
template <typename ELFT>
void ELFObjectWriter::emitRela(Module &Module, ELFSection *S,
                               MemoryRegion &pRegion, bool isDyn) const {
  typedef typename ELFT::Rela ElfXX_Rela;
  typedef typename ELFT::Addr ElfXX_Addr;
  typedef typename ELFT::Word ElfXX_Word;

  ElfXX_Rela *rel = reinterpret_cast<ElfXX_Rela *>(pRegion.begin());

  FragmentRef *frag_ref = nullptr;

  for (auto &R : S->getRelocations()) {
    ElfXX_Addr r_offset = (ElfXX_Addr)0;
    ElfXX_Word r_sym = (ElfXX_Word)0;

    frag_ref = R->targetRef();

    if ((isDyn || !m_Config.options().emitRelocs() ||
         m_Config.options().emitGNUCompatRelocs()) &&
        (LinkerConfig::DynObj == m_Config.codeGenType() ||
         LinkerConfig::Exec == m_Config.codeGenType())) {
      r_offset =
          static_cast<ElfXX_Addr>(frag_ref->getOutputELFSection()->addr() +
                                  frag_ref->getOutputOffset(Module));
    } else {
      r_offset = static_cast<ElfXX_Addr>(frag_ref->getOutputOffset(Module));
    }

    auto r_addend = R->addend();
    if (!isDyn) {
      if (R->symInfo()) {
        // TODO: QTOOL-102747: relocations to discarded debug sections
        // should be removed when tombstones are inserted or replaced with
        // REL_NONE. In that case getSymbolIdx should never be expected to
        // look for an invalid symbol.
        r_sym = static_cast<ElfXX_Word>(
            target().getSymbolIdx(R->symInfo()->outSymbol(), true));
        if (shouldEmitReloc(R)) {
          emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset, r_addend);
          ++rel;
        }
      }
      continue;
    }

    GNULDBackend::DynRelocType dynRelocType = target().getDynRelocType(R);

    switch (dynRelocType) {
    case GNULDBackend::DynRelocType::DEFAULT:
    case GNULDBackend::DynRelocType::JMP_SLOT:
    case GNULDBackend::DynRelocType::GLOB_DAT:
    case GNULDBackend::DynRelocType::TPREL_GLOBAL:
    case GNULDBackend::DynRelocType::WORD_DEPOSIT: {
      r_sym = static_cast<ElfXX_Word>(
          target().getDynSymbolIdx(R->symInfo()->outSymbol()));

      emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset, r_addend);
      break;
    }
    case GNULDBackend::DynRelocType::RELATIVE: {
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset,
                           target().getRelocator()->getSymValue(R) + r_addend);
      // llvm::errs() << "Writer Name : " << R->symInfo()->name() << " Addend :
      // " << r_addend << "\n";
    } break;

    case GNULDBackend::DynRelocType::TLSDESC_GLOBAL:
    case GNULDBackend::DynRelocType::DTPMOD_GLOBAL:
    case GNULDBackend::DynRelocType::DTPREL_GLOBAL: {
      if (R->symInfo())
        r_sym = static_cast<ElfXX_Word>(
            target().getDynSymbolIdx(R->symInfo()->outSymbol()));
      emitRelocation<ELFT>(*rel, R->type(), r_sym, r_offset, r_addend);
      break;
    }
    case GNULDBackend::DynRelocType::TLSDESC_LOCAL:
    case GNULDBackend::DynRelocType::DTPMOD_LOCAL:
    case GNULDBackend::DynRelocType::DTPREL_LOCAL:
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset, 0);
      break;

    case GNULDBackend::DynRelocType::TPREL_LOCAL:
      emitRelocation<ELFT>(*rel, R->type(), 0, r_offset,
                           target().getRelocator()->getSymValue(R) + r_addend);
      break;
    }
    ++rel;
  }
}

/// getSectEntrySize - compute ElfXX_Shdr::sh_entsize
template <typename ELFT>
uint64_t ELFObjectWriter::getSectEntrySize(ELFSection *pSection) const {
  typedef typename ELFT::Word ElfXX_Word;
  typedef typename ELFT::Sym ElfXX_Sym;
  typedef typename ELFT::Rel ElfXX_Rel;
  typedef typename ELFT::Rela ElfXX_Rela;
  typedef typename ELFT::Dyn ElfXX_Dyn;

  if (pSection->isGroupKind())
    return sizeof(llvm::ELF::Elf32_Word);
  if (llvm::ELF::SHT_DYNSYM == pSection->getType() ||
      llvm::ELF::SHT_SYMTAB == pSection->getType())
    return sizeof(ElfXX_Sym);
  if (pSection->isRel())
    return sizeof(ElfXX_Rel);
  if (pSection->isRela())
    return sizeof(ElfXX_Rela);
  if (llvm::ELF::SHT_HASH == pSection->getType() ||
      llvm::ELF::SHT_GNU_HASH == pSection->getType() ||
      llvm::ELF::SHT_SYMTAB_SHNDX == pSection->getType())
    return sizeof(ElfXX_Word);
  if (llvm::ELF::SHT_DYNAMIC == pSection->getType())
    return sizeof(ElfXX_Dyn);
  // FIXME: We should get the entsize from input since the size of each
  // character is specified in the section header's sh_entsize field.
  // For example, traditional string is 0x1, UCS-2 is 0x2, ... and so on.
  // Ref: http://www.sco.com/developers/gabi/2003-12-17/ch4.sheader.html
  if (pSection->getFlags() & llvm::ELF::SHF_STRINGS)
    return pSection->getEntSize();
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
  if (m_Config.isLinkPartial() && llvm::ELF::SHF_LINK_ORDER & S->getFlags())
    return S->getLink()->getOutputSection()->getSection()->getIndex();
  if (S->isRelocationSection()) {
    if (S->getKind() != LDFileFormat::DynamicRelocation)
      Link = target().getOutputFormat()->getSymTab();
    else
      Link = target().getOutputFormat()->getDynSymTab();
  }
  if (!Link)
    return m_Backend.getSectLink(S);

  if (Link->isIgnore() || Link->isDiscard())
    return 0;
  return Link->getIndex();
}

/// getSectInfo - compute ElfXX_Shdr::sh_info
uint64_t ELFObjectWriter::getSectInfo(ELFSection *pSection) const {
  if (pSection->isGroupKind())
    return m_Backend.getSymbolIdx(pSection->getSymbol());

  if (llvm::ELF::SHT_SYMTAB == pSection->getType() ||
      llvm::ELF::SHT_DYNSYM == pSection->getType())
    return pSection->getInfo();

  if (pSection->isRelocationSection()) {
    auto *info_link = llvm::dyn_cast_or_null<ELFSection>(pSection->getLink());
    if (nullptr != info_link) {
      uint32_t Idx = m_Backend.getSectionIdx(info_link);
      if (Idx != UINT32_MAX)
        return Idx;
      Idx = m_Backend.getSectionIdx(info_link->getOutputELFSection());
      ASSERT(Idx != UINT32_MAX,
             "Relocation section " + pSection->name().str() + "is not linked");
      if (Idx != UINT32_MAX)
        return Idx;
    }
  }

  return 0x0;
}

/// getLastStartOffset
template <typename ELFT>
uint64_t ELFObjectWriter::getLastStartOffset(const Module &pModule) const {
  const ELFSection *lastSect = pModule.back();
  assert(lastSect != nullptr);
  if (ELFT::Is64Bits)
    return llvm::alignTo<64>(lastSect->offset() + lastSect->size());
  else
    return llvm::alignTo<32>(lastSect->offset() + lastSect->size());
}

/// emitGroupData
void ELFObjectWriter::emitGroup(ELFSection *S, MemoryRegion &pRegion) {
  size_t cur_offset = 0;
  for (auto &frag : S->getFragmentList()) {
    if (frag->getKind() != Fragment::Region)
      continue;
    RegionFragment &region_frag = llvm::cast<RegionFragment>(*frag);
    const uint8_t *from = (const uint8_t *)region_frag.getRegion().data();
    size_t groupDataSize =
        region_frag.getRegion().size() / sizeof(llvm::ELF::Elf32_Word);
    // copy the contents
    memcpy(pRegion.begin() + cur_offset, from, frag->size());
    void *groupData = (void *)(pRegion.begin() + cur_offset);
    auto si = S->getGroupSections().begin();
    auto se = S->getGroupSections().end();
    for (size_t index = 1; index < groupDataSize; ++index) {
      if (si == se)
        break;
      uint32_t sectionIdx = (*si)->getOutputELFSection()->getIndex();
      ((uint32_t *)groupData)[index] = sectionIdx;
      ++si;
    }
    cur_offset += frag->size();
  }
}

eld::Expected<void> ELFObjectWriter::emitSection(ELFSection *S,
                                                 MemoryRegion &pRegion) const {
  for (auto &frag : S->getFragmentList()) {
    eld::Expected<void> expEmit = frag->emit(pRegion, m_Backend.getModule());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expEmit);
  }
  return {};
}

bool ELFObjectWriter::shouldEmitReloc(const Relocation *R) const {
  ResolveInfo *RI = R->symInfo();
  if (RI == ResolveInfo::Null())
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
    const Module &pModule) const;
template size_t ELFObjectWriter::getOutputSize<llvm::object::ELF64LE>(
    const Module &pModule) const;
template void ELFObjectWriter::writeELFHeader<llvm::object::ELF32LE>(
    LinkerConfig &pConfig, const Module &pModule,
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::writeELFHeader<llvm::object::ELF64LE>(
    LinkerConfig &pConfig, const Module &pModule,
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::emitSectionHeader<llvm::object::ELF32LE>(
    const Module &pModule, LinkerConfig &pConfig,
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::emitSectionHeader<llvm::object::ELF64LE>(
    const Module &pModule, LinkerConfig &pConfig,
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::emitProgramHeader<llvm::object::ELF32LE>(
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::emitProgramHeader<llvm::object::ELF64LE>(
    llvm::FileOutputBuffer &pOutput) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF32LE>(
    Module &Module, ELFSection *pSection, MemoryRegion &pRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRelocation<llvm::object::ELF64LE>(
    Module &Module, ELFSection *pSection, MemoryRegion &pRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRel<llvm::object::ELF32LE>(
    Module &Module, ELFSection *pSection, MemoryRegion &pRegion,
    bool isDyn) const;
template void ELFObjectWriter::emitRela<llvm::object::ELF64LE>(
    Module &Module, ELFSection *pSection, MemoryRegion &pRegion,
    bool isDyn) const;
template uint64_t ELFObjectWriter::getSectEntrySize<llvm::object::ELF32LE>(
    ELFSection *pSection) const;
template uint64_t ELFObjectWriter::getSectEntrySize<llvm::object::ELF64LE>(
    ELFSection *pSection) const;
template uint64_t ELFObjectWriter::getLastStartOffset<llvm::object::ELF32LE>(
    const Module &pModule) const;
template uint64_t ELFObjectWriter::getLastStartOffset<llvm::object::ELF64LE>(
    const Module &pModule) const;
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
