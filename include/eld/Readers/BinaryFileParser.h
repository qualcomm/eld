//===- BinaryFileParser.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_NEWREADERS_BINARYFILEPARSER_H
#define ELD_NEWREADERS_BINARYFILEPARSER_H
#include "eld/PluginAPI/Expected.h"

namespace eld {
class ELFSection;
class InputFile;
class Module;

/// BinaryFileParser parses binary format input files.
///
/// Parsing binary format input files is very simple. It consists of:
/// - Creating a section '.data' that contains the contents of the input file
///   as it is.
/// - Creating identification symbols to make the content usable by the
///   program: '_binary_${FileName}_start', '_binary_${FileName}_end' and
///   '_binary_${FileName}_size'.
class BinaryFileParser {
public:
  BinaryFileParser(Module &module) : m_Module(module) {}

  eld::Expected<void> parseFile(InputFile &inputFile);

private:
  /// Returns the symbol prefix for '_start', '_end' and '_size' identification
  /// symbols.
  std::string getSymPrefix(const InputFile &inputFile) const;

  /// Adds _binary_${File}_start, _binary_${File}_end, and _binary_${File}_size
  /// symbols.
  void addDescriptionSymbols(InputFile &inputFile, ELFSection *S);

  /// Creates the data section for binary format input file. The data section
  /// contains the binary file contents.
  ELFSection *createDataSection(InputFile &inputFile);
  Module &m_Module;
};

} // namespace eld
#endif
