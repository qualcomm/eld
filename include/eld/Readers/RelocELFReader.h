//===- RelocELFReader.h----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_RELOCELFREADER_H
#define ELD_READERS_RELOCELFREADER_H
#include "eld/Readers/ELFReader.h"
#include "eld/Readers/Relocation.h"

namespace eld {
class ELFSection;
class InputFile;
class LDSymbol;
class RelocMap;

/// RelocELFReader provides low-level functions to read dynamic input ELF
/// files. It internally utilizes llvm::object::ELFFile to parse the elf input
/// files.
///
/// Each instance of the object can only be used to read one input
/// file.
///
/// RelocELFReader propagates all the errors to the caller function using
/// eld::Expected.
template <class ELFT> class RelocELFReader : public ELFReader<ELFT> {
public:
  /// Creates and returns an instance of RelocELFReader<ELFT>.
  static eld::Expected<std::unique_ptr<RelocELFReader<ELFT>>>
  Create(Module &module, InputFile &inputFile);

  /// defaulted copy-constructor.
  RelocELFReader(const RelocELFReader &) = default;
  // defaulted move constructor.
  RelocELFReader(RelocELFReader &&) noexcept = default;

  // delete assignment operators.
  RelocELFReader &operator=(const RelocELFReader &) = delete;
  RelocELFReader &operator=(RelocELFReader &&) = delete;

  /// Create an eld::ELFSection object for the raw section rawSectHdr.
  eld::Expected<ELFSection *>
  createSection(typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) override;

  /// Creates refined section headers by reading raw section headers.
  ///
  /// This function also adds references to all the created refined section
  /// headers in the input file.
  eld::Expected<bool> readSectionHeaders() override;

  /// Returns the group signature.
  eld::Expected<ResolveInfo *>
  computeGroupSignature(const ELFSection *S) const override;

  /// Returns section indices of the group members.
  eld::Expected<std::vector<uint32_t>>
  getGroupMemberIndices(const ELFSection *S) const override;

  /// Returns the group flag.
  eld::Expected<uint32_t> getGroupFlag(const ELFSection *S) const override;

  /// Reads compressed section.
  eld::Expected<bool> readCompressedSection(ELFSection *S) override;

  /// Reads merged string section.
  eld::Expected<bool> readMergeStringSection(ELFSection *S) override;

  /// Reads one group section.
  eld::Expected<bool> readOneGroup(ELFSection *S) override;

  eld::Expected<bool> readRelocationSection(ELFSection *RS) override;

protected:
  explicit RelocELFReader(Module &module, InputFile &inputFile,
                          plugin::DiagnosticEntry &diagEntry);

  /// Sets the correct origin for post LTO common symbols.
  void setPostLTOCommonSymbolsOrigin() const override;

  /// Computes symbold name.
  eld::Expected<llvm::StringRef>
  computeSymbolName(const typename ELFReader<ELFT>::Elf_Sym &rawSym,
                    const ELFSection *S, llvm::StringRef stringTable,
                    uint32_t st_shndx, ResolveInfo::Type ldType) const override;

  template <bool isRela>
  eld::Expected<bool> readRelocationSection(ELFSection *RS);

  LDSymbol *fixWrapSyms(LDSymbol *sym);

  LDSymbol *createUndefReference(llvm::StringRef symName);
};
} // namespace eld
#endif
