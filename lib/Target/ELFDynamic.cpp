//===- ELFDynamic.cpp------------------------------------------------------===//
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

#include "eld/Target/ELFDynamic.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"

using namespace eld;
using namespace elf_dynamic;

//===----------------------------------------------------------------------===//
// elf_dynamic::EntryIF
//===----------------------------------------------------------------------===//
EntryIF::EntryIF() {}

EntryIF::~EntryIF() {}

//===----------------------------------------------------------------------===//
// ELFDynamic
//===----------------------------------------------------------------------===//
ELFDynamic::ELFDynamic(GNULDBackend &pParent, LinkerConfig &pConfig)
    : m_pEntryFactory(nullptr), m_Idx(0), m_Backend(pParent),
      m_Config(pConfig) {
  // FIXME: support big-endian machine.
  if (m_Config.targets().is32Bits()) {
    if (m_Config.targets().isLittleEndian())
      m_pEntryFactory = new Entry<32, true>();
  } else if (m_Config.targets().is64Bits()) {
    if (m_Config.targets().isLittleEndian())
      m_pEntryFactory = new Entry<64, true>();
  } else {
    m_Config.raise(Diag::unsupported_bitclass)
        << m_Config.targets().triple().str() << m_Config.targets().bitclass();
  }
}

ELFDynamic::~ELFDynamic() {
  if (nullptr != m_pEntryFactory)
    delete m_pEntryFactory;

  EntryListType::iterator entry, entryEnd = m_EntryList.end();
  for (entry = m_EntryList.begin(); entry != entryEnd; ++entry) {
    if (nullptr != *entry)
      delete (*entry);
  }

  entryEnd = m_NeedList.end();
  for (entry = m_NeedList.begin(); entry != entryEnd; ++entry) {
    if (nullptr != *entry)
      delete (*entry);
  }
}

size_t ELFDynamic::size() const {
  return (m_NeedList.size() + m_EntryList.size());
}

size_t ELFDynamic::numOfBytes() const { return size() * entrySize(); }

size_t ELFDynamic::entrySize() const { return m_pEntryFactory->size(); }

std::string ELFDynamic::TagToString(uint64_t Tag) {
#define INTOTAGSTR(ns, T)                                                      \
  if (Tag == ns::T)                                                            \
    return #T;

  INTOTAGSTR(llvm::ELF, DT_SONAME);
  INTOTAGSTR(llvm::ELF, DT_SYMBOLIC);
  INTOTAGSTR(llvm::ELF, DT_INIT);
  INTOTAGSTR(llvm::ELF, DT_FINI);
  INTOTAGSTR(llvm::ELF, DT_PREINIT_ARRAY);
  INTOTAGSTR(llvm::ELF, DT_PREINIT_ARRAYSZ);
  INTOTAGSTR(llvm::ELF, DT_INIT_ARRAY);
  INTOTAGSTR(llvm::ELF, DT_INIT_ARRAYSZ);
  INTOTAGSTR(llvm::ELF, DT_FINI_ARRAY);
  INTOTAGSTR(llvm::ELF, DT_HASH);
  INTOTAGSTR(llvm::ELF, DT_GNU_HASH);
  INTOTAGSTR(llvm::ELF, DT_SYMTAB);
  INTOTAGSTR(llvm::ELF, DT_SYMENT);
  INTOTAGSTR(llvm::ELF, DT_STRTAB);
  INTOTAGSTR(llvm::ELF, DT_STRSZ);
  INTOTAGSTR(llvm::ELF, DT_PLTGOT);
  INTOTAGSTR(llvm::ELF, DT_PLTREL);
  INTOTAGSTR(llvm::ELF, DT_JMPREL);
  INTOTAGSTR(llvm::ELF, DT_PLTRELSZ);
  INTOTAGSTR(llvm::ELF, DT_REL);
  INTOTAGSTR(llvm::ELF, DT_RELSZ);
  INTOTAGSTR(llvm::ELF, DT_RELENT);
  INTOTAGSTR(llvm::ELF, DT_RELA);
  INTOTAGSTR(llvm::ELF, DT_RELASZ);
  INTOTAGSTR(llvm::ELF, DT_RELAENT);
  INTOTAGSTR(llvm::ELF, DT_BIND_NOW);
  INTOTAGSTR(llvm::ELF, DT_FLAGS);
  INTOTAGSTR(llvm::ELF, DT_TEXTREL);
  INTOTAGSTR(llvm::ELF, DT_FLAGS_1);
  INTOTAGSTR(llvm::ELF, DT_DEBUG);
  INTOTAGSTR(llvm::ELF, DT_NULL);
  return "";
}

void ELFDynamic::reserveOne(uint64_t pTag) {
  // llvm::errs() << "R : " << TagToString(pTag) << "\n";;
  assert(nullptr != m_pEntryFactory);
  m_EntryList.push_back(m_pEntryFactory->clone());
}

void ELFDynamic::applyOne(uint64_t pTag, uint64_t pValue) {
  // llvm::errs() << "A : " << TagToString(pTag) << "\n";
  assert(m_Idx < m_EntryList.size());
  m_EntryList[m_Idx]->setValue(pTag, pValue);
  ++m_Idx;
}

/// reserveEntries - reserve entries
void ELFDynamic::reserveEntries(ELFFileFormat &pFormat, Module &pModule) {
  if (LinkerConfig::DynObj == m_Config.codeGenType()) {
    // DT_SONAME is the 0th entry in the dynamic section.
    if (pModule.getSection(".dynstr") && !m_Config.options().soname().empty()) {
      reserveOne(llvm::ELF::DT_SONAME); // DT_SONAME
      applySoname(pFormat.addStringToDynStrTab(m_Config.options().soname()));
    }

    if (m_Config.options().bsymbolic())
      reserveOne(llvm::ELF::DT_SYMBOLIC); // DT_SYMBOLIC
  }

  if (pModule.getSection(".init") || m_Config.options().dtinit().size())
    reserveOne(llvm::ELF::DT_INIT); // DT_INIT

  if (pModule.getSection(".fini") || m_Config.options().dtfini().size())
    reserveOne(llvm::ELF::DT_FINI); // DT_FINI

  if (pModule.getSection(".preinit_array")) {
    reserveOne(llvm::ELF::DT_PREINIT_ARRAY);   // DT_PREINIT_ARRAY
    reserveOne(llvm::ELF::DT_PREINIT_ARRAYSZ); // DT_PREINIT_ARRAYSZ
  }

  if (pModule.getSection(".init_array")) {
    reserveOne(llvm::ELF::DT_INIT_ARRAY);   // DT_INIT_ARRAY
    reserveOne(llvm::ELF::DT_INIT_ARRAYSZ); // DT_INIT_ARRAYSZ
  }

  if (pModule.getSection(".fini_array")) {
    reserveOne(llvm::ELF::DT_FINI_ARRAY);   // DT_FINI_ARRAY
    reserveOne(llvm::ELF::DT_FINI_ARRAYSZ); // DT_FINI_ARRAYSZ
  }

  if (pModule.getSection(".hash"))
    reserveOne(llvm::ELF::DT_HASH); // DT_HASH

  // FIXME: use llvm enum constant
  if (pModule.getSection(".gnu.hash"))
    reserveOne(0x6ffffef5); // DT_GNU_HASH

  if (pModule.getSection(".dynsym")) {
    reserveOne(llvm::ELF::DT_SYMTAB); // DT_SYMTAB
    reserveOne(llvm::ELF::DT_SYMENT); // DT_SYMENT
  }

  if (pModule.getSection(".dynstr")) {
    reserveOne(llvm::ELF::DT_STRTAB); // DT_STRTAB
    reserveOne(llvm::ELF::DT_STRSZ);  // DT_STRSZ
  }

  if (!m_Config.options().isCompactDyn() && m_Backend.getGOTPLT() &&
      m_Backend.getGOTPLT()->size() != 0) {
    assert(m_Backend.getGOTPLT()->hasVMA());
    reserveOne(llvm::ELF::DT_PLTGOT);
  }

  reserveTargetEntries();

  if (pModule.getSection(".rel.plt") || pModule.getSection(".rela.plt")) {
    reserveOne(llvm::ELF::DT_PLTREL);   // DT_PLTREL
    reserveOne(llvm::ELF::DT_JMPREL);   // DT_JMPREL
    reserveOne(llvm::ELF::DT_PLTRELSZ); // DT_PLTRELSZ
  }

  if (pModule.getSection(".rel.dyn")) {
    reserveOne(llvm::ELF::DT_REL);    // DT_REL
    reserveOne(llvm::ELF::DT_RELSZ);  // DT_RELSZ
    reserveOne(llvm::ELF::DT_RELENT); // DT_RELENT
  }

  if (pModule.getSection(".rela.dyn")) {
    reserveOne(llvm::ELF::DT_RELA);    // DT_RELA
    reserveOne(llvm::ELF::DT_RELASZ);  // DT_RELASZ
    reserveOne(llvm::ELF::DT_RELAENT); // DT_RELAENT
  }

  if (m_Config.options().hasNow() && !m_Config.options().hasNewDTags())
    reserveOne(llvm::ELF::DT_BIND_NOW);

  // All values for new flags go here.
  uint64_t dt_flags = 0x0;
  if (m_Config.options().hasNow())
    dt_flags |= llvm::ELF::DF_BIND_NOW;
  if (m_Config.options().bsymbolic())
    dt_flags |= llvm::ELF::DF_SYMBOLIC;
  if (m_Backend.hasTextRel())
    dt_flags |= llvm::ELF::DF_TEXTREL;
  if (m_Backend.hasStaticTLS() &&
      (LinkerConfig::DynObj == m_Config.codeGenType()))
    dt_flags |= llvm::ELF::DF_STATIC_TLS;

  if ((m_Config.options().hasNewDTags() && dt_flags != 0x0) ||
      (dt_flags & llvm::ELF::DF_STATIC_TLS) != 0x0)
    reserveOne(llvm::ELF::DT_FLAGS);

  if (m_Backend.hasTextRel())
    reserveOne(llvm::ELF::DT_TEXTREL);

  if (m_Config.options().hasNow() || m_Config.options().hasNoDelete() ||
      m_Config.options().hasGlobal() || m_Config.options().isPIE())
    reserveOne(llvm::ELF::DT_FLAGS_1);

  if (m_Backend.hasTextRel())
    reserveOne(llvm::ELF::DT_TEXTREL); // DT_TEXTREL

  if (m_Backend.getGNUVerSymSection())
    reserveOne(llvm::ELF::DT_VERSYM);

  if (m_Backend.getGNUVerDefSection()) {
    reserveOne(llvm::ELF::DT_VERDEF);
    reserveOne(llvm::ELF::DT_VERDEFNUM);
  }

  reserveOne(llvm::ELF::DT_DEBUG); // for Debugging
  reserveOne(llvm::ELF::DT_NULL);  // for DT_NULL
}

/// applyEntries - apply entries
void ELFDynamic::applyEntries(const ELFFileFormat &pFormat,
                              const Module &pModule) {
  if (LinkerConfig::DynObj == m_Config.codeGenType() &&
      m_Config.options().bsymbolic()) {
    applyOne(llvm::ELF::DT_SYMBOLIC, 0x0); // DT_SYMBOLIC
  }

  if (pModule.getSection(".init") && !m_Config.options().dtinit().size())
    applyOne(llvm::ELF::DT_INIT,
             pModule.getSection(".init")->addr()); // DT_INIT
  else if (m_Config.options().dtinit().size()) {
    const LDSymbol *symbol =
        pModule.getNamePool().findSymbol(m_Config.options().dtinit());
    assert(nullptr != symbol);
    applyOne(llvm::ELF::DT_INIT, symbol->value());
  }

  if (pModule.getSection(".fini") && !m_Config.options().dtfini().size())
    applyOne(llvm::ELF::DT_FINI,
             pModule.getSection(".fini")->addr()); // DT_FINI
  else if (m_Config.options().dtfini().size()) {
    const LDSymbol *symbol =
        pModule.getNamePool().findSymbol(m_Config.options().dtfini());
    assert(nullptr != symbol);
    applyOne(llvm::ELF::DT_FINI, symbol->value()); // DT_FINI
  }

  if (pModule.getSection(".preinit_array")) {
    // DT_PREINIT_ARRAY
    applyOne(llvm::ELF::DT_PREINIT_ARRAY,
             pModule.getSection(".preinit_array")->addr());
    // DT_PREINIT_ARRAYSZ
    applyOne(llvm::ELF::DT_PREINIT_ARRAYSZ,
             pModule.getSection(".preinit_array")->size());
  }

  if (pModule.getSection(".init_array")) {
    // DT_INIT_ARRAY
    applyOne(llvm::ELF::DT_INIT_ARRAY,
             pModule.getSection(".init_array")->addr());
    // DT_INIT_ARRAYSZ
    applyOne(llvm::ELF::DT_INIT_ARRAYSZ,
             pModule.getSection(".init_array")->size());
  }

  if (pModule.getSection(".fini_array")) {
    // DT_FINI_ARRAY
    applyOne(llvm::ELF::DT_FINI_ARRAY,
             pModule.getSection(".fini_array")->addr());
    // DT_FINI_ARRAYSZ
    applyOne(llvm::ELF::DT_FINI_ARRAYSZ,
             pModule.getSection(".fini_array")->size());
  }

  if (pModule.getSection(".hash"))
    applyOne(llvm::ELF::DT_HASH,
             pModule.getSection(".hash")->addr()); // DT_HASH

  if (pModule.getSection(".gnu.hash"))
    applyOne(0x6ffffef5,
             pModule.getSection(".gnu.hash")->addr()); // DT_GNU_HASH

  if (pModule.getSection(".dynsym")) {
    applyOne(llvm::ELF::DT_SYMTAB,
             pModule.getSection(".dynsym")->addr()); // DT_SYMTAB
    applyOne(llvm::ELF::DT_SYMENT, symbolSize());    // DT_SYMENT
  }

  if (pModule.getSection(".dynstr")) {
    applyOne(llvm::ELF::DT_STRTAB,
             pModule.getSection(".dynstr")->addr()); // DT_STRTAB
    applyOne(llvm::ELF::DT_STRSZ,
             pModule.getSection(".dynstr")->size()); // DT_STRSZ
  }

  if (!m_Config.options().isCompactDyn())
    if (const ELFSection *GOTPLT = m_Backend.getGOTPLT())
      if (GOTPLT->size() != 0)
        // DT_PLTGOT always points to the GOTPLT section. Glad that the
        // linker treats .got.plt section as internal. DT_PLTGOT is needed
        // only by lazy binding. Note that both ld and lld create
        // DT_PLTGOT even with lazy binding and the image crashes without
        // it on riscv qemu.
        applyOne(llvm::ELF::DT_PLTGOT, GOTPLT->addr());

  applyTargetEntries();

  if (pModule.getSection(".rel.plt")) {
    applyOne(llvm::ELF::DT_PLTREL, llvm::ELF::DT_REL); // DT_PLTREL
    applyOne(llvm::ELF::DT_JMPREL,
             pModule.getSection(".rel.plt")->addr()); // DT_JMPREL
    applyOne(llvm::ELF::DT_PLTRELSZ,
             pModule.getSection(".rel.plt")->size()); // DT_PLTRELSZ
  } else if (pModule.getSection(".rela.plt")) {
    applyOne(llvm::ELF::DT_PLTREL, llvm::ELF::DT_RELA); // DT_PLTREL
    applyOne(llvm::ELF::DT_JMPREL,
             pModule.getSection(".rela.plt")->addr()); // DT_JMPREL
    applyOne(llvm::ELF::DT_PLTRELSZ,
             pModule.getSection(".rela.plt")->size()); // DT_PLTRELSZ
  }

  if (pModule.getSection(".rel.dyn")) {
    applyOne(llvm::ELF::DT_REL,
             pModule.getSection(".rel.dyn")->addr()); // DT_REL
    applyOne(llvm::ELF::DT_RELSZ,
             pModule.getSection(".rel.dyn")->size());           // DT_RELSZ
    applyOne(llvm::ELF::DT_RELENT, m_pEntryFactory->relSize()); // DT_RELENT
  }

  if (pModule.getSection(".rela.dyn")) {
    applyOne(llvm::ELF::DT_RELA,
             pModule.getSection(".rela.dyn")->addr()); // DT_RELA
    applyOne(llvm::ELF::DT_RELASZ,
             pModule.getSection(".rela.dyn")->size());            // DT_RELASZ
    applyOne(llvm::ELF::DT_RELAENT, m_pEntryFactory->relaSize()); // DT_RELAENT
  }

  if (m_Backend.hasTextRel()) {
    applyOne(llvm::ELF::DT_TEXTREL, 0x0); // DT_TEXTREL

    if (m_Config.options().warnSharedTextrel() &&
        LinkerConfig::DynObj == m_Config.codeGenType())
      m_Config.raise(Diag::warn_shared_textrel);
  }

  if (m_Config.options().hasNow() && !m_Config.options().hasNewDTags())
    applyOne(llvm::ELF::DT_BIND_NOW, 1);

  // All values for new flags go here.
  uint64_t dt_flags = 0x0;
  if (m_Config.options().bsymbolic())
    dt_flags |= llvm::ELF::DF_SYMBOLIC;
  if (m_Config.options().hasNow())
    dt_flags |= llvm::ELF::DF_BIND_NOW;
  if (m_Backend.hasTextRel())
    dt_flags |= llvm::ELF::DF_TEXTREL;
  if (m_Backend.hasStaticTLS() &&
      (LinkerConfig::DynObj == m_Config.codeGenType()))
    dt_flags |= llvm::ELF::DF_STATIC_TLS;

  if ((m_Config.options().hasNewDTags() && dt_flags != 0x0) ||
      (dt_flags & llvm::ELF::DF_STATIC_TLS) != 0)
    applyOne(llvm::ELF::DT_FLAGS, dt_flags);

  uint64_t dt_flags_1 = 0x0;
  if (m_Config.options().isPIE())
    dt_flags_1 |= llvm::ELF::DF_1_PIE;
  if (m_Config.options().hasNow())
    dt_flags_1 |= llvm::ELF::DF_1_NOW;
  if (LinkerConfig::DynObj == m_Config.codeGenType()) {
    if (m_Config.options().hasNoDelete())
      dt_flags_1 |= llvm::ELF::DF_1_NODELETE;
  }
  if (m_Config.options().hasGlobal())
    dt_flags_1 |= llvm::ELF::DF_1_GLOBAL;
  if (dt_flags_1 != 0x0)
    applyOne(llvm::ELF::DT_FLAGS_1, dt_flags_1);

  if (ELFSection *S = m_Backend.getGNUVerSymSection()) {
    applyOne(llvm::ELF::DT_VERSYM, S->addr());
  }

  if (ELFSection *S = m_Backend.getGNUVerDefSection()) {
    applyOne(llvm::ELF::DT_VERDEF, S->addr());
    // Def count equals section sh_info
    applyOne(llvm::ELF::DT_VERDEFNUM, S->getInfo());
  }

  if (!m_Config.options().isCompactDyn())
    applyOne(llvm::ELF::DT_DEBUG, 0x0); // for DT_DEBUG

  applyOne(llvm::ELF::DT_NULL, 0x0); // for DT_NULL
}

/// symbolSize
size_t ELFDynamic::symbolSize() const { return m_pEntryFactory->symbolSize(); }

/// reserveNeedEntry - reserve on DT_NEED entry.
elf_dynamic::EntryIF * ELFDynamic::reserveNeedEntry() {
  m_NeedList.push_back(m_pEntryFactory->clone());
  return m_NeedList.back();
}

/// emit
void ELFDynamic::emit(const ELFSection &pSection, MemoryRegion &pRegion) const {
  if (pRegion.size() < pSection.size()) {
    llvm::report_fatal_error(llvm::Twine("the given memory is smaller") +
                             llvm::Twine(" than the section's demand.\n"));
  }

  uint8_t *address = (uint8_t *)pRegion.begin();
  EntryListType::const_iterator entry, entryEnd = m_NeedList.end();
  for (entry = m_NeedList.begin(); entry != entryEnd; ++entry)
    address += (*entry)->emit(address);

  entryEnd = m_EntryList.end();
  for (entry = m_EntryList.begin(); entry != entryEnd; ++entry)
    address += (*entry)->emit(address);
}

void ELFDynamic::applySoname(uint64_t pStrTabIdx) {
  applyOne(llvm::ELF::DT_SONAME, pStrTabIdx); // DT_SONAME
}
