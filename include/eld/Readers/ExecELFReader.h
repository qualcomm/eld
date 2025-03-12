//===- ExecELFReader.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#ifndef ELD_READERS_EXECELFREADER_H
#define ELD_READERS_EXECELFREADER_H
#include "eld/Readers/ELFReader.h"
#include "eld/Readers/ELFReaderBase.h"

namespace eld {
template <class ELFT> class ExecELFReader : public ELFReader<ELFT> {
public:
  ExecELFReader(const ExecELFReader &) = default;
  ExecELFReader(ExecELFReader &&) noexcept = default;

  ExecELFReader &operator=(const ExecELFReader &) = delete;
  ExecELFReader &operator=(ExecELFReader &) = delete;

  static eld::Expected<std::unique_ptr<ExecELFReader<ELFT>>>
  Create(Module &module, InputFile &inputFile);

  /// Creates refined section headers by reading raw section headers.
  ///
  /// This function also adds references to all the created refined section
  /// headers in the input file.
  eld::Expected<bool> readSectionHeaders() override;

  /// Create an eld::ELFSection object for the raw section rawSectHdr.
  eld::Expected<ELFSection *>
  createSection(typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) override;

  eld::Expected<bool> readRelocationSection(ELFSection *RS) override;

  eld::Expected<void> readRelocations();

protected:
  explicit ExecELFReader(Module &module, InputFile &inputFile,
                         plugin::DiagnosticEntry &diagEntry);

  template <bool isRela>
  eld::Expected<bool> readRelocationSection(ELFSection *RS);
};
} // namespace eld

#endif