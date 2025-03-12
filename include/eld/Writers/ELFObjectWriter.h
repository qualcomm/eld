//===- ELFObjectWriter.h---------------------------------------------------===//
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
#ifndef ELD_WRITERS_ELFOBJECTWRITER_H
#define ELD_WRITERS_ELFOBJECTWRITER_H
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/Relocation.h"
#include "eld/Support/MemoryRegion.h"
#include "llvm/Support/FileOutputBuffer.h"

namespace eld {

class Module;
class LinkerConfig;
class GNULDBackend;
class FragmentLinker;
class Relocation;
class ELFSection;
class Output;

/** \class ELFObjectWriter
 *  \brief ELFObjectWriter writes the target-independent parts of object files.
 *  ELFObjectWriter reads a ELDFile and writes into raw_ostream
 *
 */
class ELFObjectWriter {
public:
  ELFObjectWriter(GNULDBackend &pBackend, LinkerConfig &pConfig);

  ~ELFObjectWriter();

  std::error_code writeObject(Module &pModule, llvm::FileOutputBuffer &pOutput);

  // write timing stats into .note.qc.timing when -emit-timing-stats-in-output
  // enabled
  void writeLinkTimeStats(Module &pModule, uint64_t beginningOfTime,
                          uint64_t duration);

  // Helper function, also used by compression.
  eld::Expected<void> writeRegion(Module &pModule, ELFSection *section,
                                  MemoryRegion &region);

  template <typename ELFT> size_t getOutputSize(const Module &pModule) const;

  eld::Expected<void> writeSection(Module &pModule,
                                   llvm::FileOutputBuffer &pOutput,
                                   ELFSection *section);

private:
  GNULDBackend &target() { return m_Backend; }

  const GNULDBackend &target() const { return m_Backend; }

  // writeELFHeader - emit ElfXX_Ehdr
  template <typename ELFT>
  void writeELFHeader(LinkerConfig &pConfig, const Module &pModule,
                      llvm::FileOutputBuffer &pOutput) const;

  uint64_t getEntryPoint(LinkerConfig &pConfig, const Module &pModule) const;

  // emitSectionHeader - emit ElfXX_Shdr
  template <typename ELFT>
  void emitSectionHeader(const Module &pModule, LinkerConfig &pConfig,
                         llvm::FileOutputBuffer &pOutput) const;

  // emitProgramHeader - emit ElfXX_Phdr
  template <typename ELFT>
  void emitProgramHeader(llvm::FileOutputBuffer &pOutput) const;

  // emitShStrTab - emit .shstrtab
  void emitShStrTab(ELFSection *shStrTab, const Module &pModule,
                    llvm::FileOutputBuffer &pOutput);

  template <typename ELFT>
  void emitRelocation(Module &Module, ELFSection *pSection,
                      MemoryRegion &pRegion, bool isDyn) const;

  // emitRel - emit ElfXX_Rel
  template <typename ELFT>
  void emitRel(Module &Module, ELFSection *S, MemoryRegion &pRegion,
               bool isDyn) const;

  // emitRela - emit ElfXX_Rela
  template <typename ELFT>
  void emitRela(Module &Module, ELFSection *S, MemoryRegion &pRegion,
                bool isDyn) const;

  // getSectEntrySize - compute ElfXX_Shdr::sh_entsize
  template <typename ELFT> uint64_t getSectEntrySize(ELFSection *S) const;

  // getSectLink - compute ElfXX_Shdr::sh_link
  uint64_t getSectLink(const ELFSection *S) const;

  // getSectInfo - compute ElfXX_Shdr::sh_info
  uint64_t getSectInfo(ELFSection *S) const;

  template <typename ELFT>
  uint64_t getLastStartOffset(const Module &pModule) const;

  eld::Expected<void> emitSection(ELFSection *pSection,
                                  MemoryRegion &pRegion) const;

  void emitGroup(ELFSection *S, MemoryRegion &pRegion);

  bool shouldEmitReloc(const Relocation *R) const;

  // Emit SHT_REL/SHT_RELA relocations
  template <typename ELFT>
  void emitRelocation(typename ELFT::Rel &pRel, Relocation::Type pType,
                      uint32_t pSymIdx, uint32_t pOffset) const;

  template <typename ELFT>
  void emitRelocation(typename ELFT::Rela &pRel, Relocation::Type pType,
                      uint32_t pSymIdx, uint32_t pOffset,
                      int32_t pAddend) const;

private:
  GNULDBackend &m_Backend;

  LinkerConfig &m_Config;
};

} // namespace eld

#endif
