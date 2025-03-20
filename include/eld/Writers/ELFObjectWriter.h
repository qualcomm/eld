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
  ELFObjectWriter(GNULDBackend &CurBackend, LinkerConfig &CurConfig);

  ~ELFObjectWriter();

  std::error_code writeObject(Module &CurModule,
                              llvm::FileOutputBuffer &CurOutput);

  // write timing stats into .note.qc.timing when -emit-timing-stats-in-output
  // enabled
  void writeLinkTimeStats(Module &CurModule, uint64_t BeginningOfTime,
                          uint64_t Duration);

  // Helper function, also used by compression.
  eld::Expected<void> writeRegion(Module &CurModule, ELFSection *Section,
                                  MemoryRegion &Region);

  template <typename ELFT> size_t getOutputSize(const Module &CurModule) const;

  eld::Expected<void> writeSection(Module &CurModule,
                                   llvm::FileOutputBuffer &CurOutput,
                                   ELFSection *Section);

private:
  GNULDBackend &target() { return Backend; }

  const GNULDBackend &target() const { return Backend; }

  // writeELFHeader - emit ElfXX_Ehdr
  template <typename ELFT>
  void writeELFHeader(LinkerConfig &CurConfig, const Module &CurModule,
                      llvm::FileOutputBuffer &CurOutput) const;

  uint64_t getEntryPoint(LinkerConfig &CurConfig,
                         const Module &CurModule) const;

  // emitSectionHeader - emit ElfXX_Shdr
  template <typename ELFT>
  void emitSectionHeader(const Module &CurModule, LinkerConfig &CurConfig,
                         llvm::FileOutputBuffer &CurOutput) const;

  // emitProgramHeader - emit ElfXX_Phdr
  template <typename ELFT>
  void emitProgramHeader(llvm::FileOutputBuffer &CurOutput) const;

  // emitShStrTab - emit .shstrtab
  void emitShStrTab(ELFSection *ShStrTab, const Module &CurModule,
                    llvm::FileOutputBuffer &CurOutput);

  template <typename ELFT>
  void emitRelocation(Module &Module, ELFSection *CurSection,
                      MemoryRegion &CurRegion, bool IsDyn) const;

  // emitRel - emit ElfXX_Rel
  template <typename ELFT>
  void emitRel(Module &Module, ELFSection *S, MemoryRegion &CurRegion,
               bool IsDyn) const;

  // emitRela - emit ElfXX_Rela
  template <typename ELFT>
  void emitRela(Module &Module, ELFSection *S, MemoryRegion &CurRegion,
                bool IsDyn) const;

  // getSectEntrySize - compute ElfXX_Shdr::sh_entsize
  template <typename ELFT> uint64_t getSectEntrySize(ELFSection *S) const;

  // getSectLink - compute ElfXX_Shdr::sh_link
  uint64_t getSectLink(const ELFSection *S) const;

  // getSectInfo - compute ElfXX_Shdr::sh_info
  uint64_t getSectInfo(ELFSection *S) const;

  template <typename ELFT>
  uint64_t getLastStartOffset(const Module &CurModule) const;

  eld::Expected<void> emitSection(ELFSection *CurSection,
                                  MemoryRegion &CurRegion) const;

  void emitGroup(ELFSection *S, MemoryRegion &CurRegion);

  bool shouldEmitReloc(const Relocation *R) const;

  // Emit SHT_REL/SHT_RELA relocations
  template <typename ELFT>
  void emitRelocation(typename ELFT::Rel &PRel, Relocation::Type CurType,
                      uint32_t CurSymIdx, uint32_t CurOffset) const;

  template <typename ELFT>
  void emitRelocation(typename ELFT::Rela &PRel, Relocation::Type CurType,
                      uint32_t CurSymIdx, uint32_t CurOffset,
                      int32_t CurAddend) const;

private:
  GNULDBackend &Backend;

  LinkerConfig &Config;
};

} // namespace eld

#endif
