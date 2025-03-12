//===- ELFRelocObjParser.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_ELFRELOCOBJPARSER_H
#define ELD_READERS_ELFRELOCOBJPARSER_H
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"
#include <memory>

namespace eld {
class Module;

/// ELFRelocObjParser parses relocatable object files.
///
/// ELFRelocObjParser is a driver reader class. It internally
/// uses an instance of RelocELFReader<ELFT> to drive the reading
/// process.
class ELFRelocObjParser {
public:
  ELFRelocObjParser(Module &module);

  // clang-format off
  /// This function does one of the two things depending on the context:
  /// - Completely parses the input file if the given input should not need be
  ///   overriden with bitcode file; Or
  /// - If the given input needs to be overriden with bitcode file, then the
  ///   function just parses the section headers, and then returns true. It
  ///   also sets 'ELFOverridenWithBC' reference variable as true. This
  ///   indicates that the caller function should initiate parsing of the
  ///   overridden bitcode file.
  ///
  /// Here, parsing consists of the following things in the same order as
  /// listed:
  /// - Verify that file is compatible with the target configuration.
  /// - Record the input file in layout printer
  /// - Read section table headers and create corresponding eld::ELFSection
  ///   objects.
  /// - Read and process sections. Some section such as group sections,
  ///   compressed sections etc needs additinal processsing.
  /// - Read symbol table headers and create corresponding eld::LDSymbol
  ///   objects.
  /// - Read groups. This involve adding group members section to the
  ///   corresponding group section.
  // clang-format on
  eld::Expected<bool> parseFile(InputFile &inputFile, bool &ELFOverridenWithBC);

  eld::Expected<bool> readRelocations(InputFile &inputFile);

private:
  eld::Expected<bool> readSectionHeaders(ELFReaderBase &ELFReader);

  eld::Expected<bool> readSections(ELFReaderBase &ELFReader);

  eld::Expected<bool> readGroupSection(ELFReaderBase &ELFReader, ELFSection *S);

  eld::Expected<bool> readLinkOnceSection(ELFReaderBase &ELFReader,
                                          ELFSection *S);

  eld::Expected<bool> readMergeStrSection(ELFReaderBase &ELFReader,
                                          ELFSection *S);

  eld::Expected<bool> readDebugSection(ELFReaderBase &ELFReader, ELFSection *S);

  eld::Expected<bool> readTimingSection(ELFReaderBase &ELFReader,
                                        ELFSection *S);

  eld::Expected<bool> readDiscardSection(ELFReaderBase &ELFReader,
                                         ELFSection *S);

  /// Read group sections.
  eld::Expected<bool> readGroups(ELFReaderBase &ELFReader);

  Module &m_Module;
};

} // namespace eld

#endif
