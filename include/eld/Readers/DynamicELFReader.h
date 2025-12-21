//===- DynamicELFReader.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_DYNAMICELFREADER_H
#define ELD_READERS_DYNAMICELFREADER_H
#include "eld/Readers/ELFReader.h"

namespace eld {
class ELFSection;
class InputFile;

/// DynamicELFReader provides low-level functions to read dynamic input ELF
/// files. It internally utilizes llvm::object::ELFFile to parse the elf input
/// files.
///
/// Each instance of the object can only be used to read one input
/// file.
///
/// DynamicELFReader propagates all the errors to the caller function using
/// eld::Expected.
template <class ELFT> class DynamicELFReader : public ELFReader<ELFT> {
public:
  /// Creates and returns an instance of DynamicELFReader<ELFT>.
  static eld::Expected<std::unique_ptr<DynamicELFReader<ELFT>>>
  Create(Module &module, InputFile &inputFile);

  /// defaulted copy-constructor.
  DynamicELFReader(const DynamicELFReader &) = default;
  // defaulted move constructor.
  DynamicELFReader(DynamicELFReader &&) noexcept = default;

  // delete assignment operators.
  DynamicELFReader &operator=(const DynamicELFReader &) = delete;
  DynamicELFReader &operator=(DynamicELFReader &&) = delete;

  /// Creates refined section headers by reading raw section headers.
  ///
  /// This function also adds references to all the created refined section
  /// headers in the input file.
  eld::Expected<bool> readSectionHeaders() override;

  eld::Expected<bool> readSymbols() override;

  /// Set dynamic object specific properties by reading raw dynamic entries.
  eld::Expected<bool> readDynamic();

  /// Set symbol alias information for dyanmic objects.
  void setSymbolsAliasInfo() override;

  eld::Expected<ELFSection *>
  createSection(typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) override;

#ifdef ELD_ENABLE_SYMBOL_VERSIONING
  void
  setSectionInInputFile(ELFSection *S,
                        typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) override;

  eld::Expected<void> readSections() override;
#endif

protected:
  explicit DynamicELFReader(Module &module, InputFile &inputFile,
                            plugin::DiagnosticEntry &diagEntry);
#ifdef ELD_ENABLE_SYMBOL_VERSIONING
private:
  eld::Expected<void> readVerDefSection();
  eld::Expected<void> readVerSymSection();
  eld::Expected<void> readVerNeedSection();
#endif
};
} // namespace eld
#endif
