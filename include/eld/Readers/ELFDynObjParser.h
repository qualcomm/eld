//===- ELFDynObjParser.h---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_ELFDYNOBJREADER_H
#define ELD_READERS_ELFDYNOBJREADER_H
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"
#include <memory>

namespace eld {
class Module;

/// ELFDynObjParser parses dynamic object files.
///
/// ELFDynObjParser is a driver reader class. It internally
/// uses an instance of DynamicELFReader<ELFT> to drive the reading
/// process.
class ELFDynObjParser {
public:
  ELFDynObjParser(Module &module);

  ~ELFDynObjParser();

  eld::Expected<bool> parseFile(InputFile &inputFile);

private:
  eld::Expected<bool> readSectionHeaders(ELFReaderBase &ELFReader);
  eld::Expected<bool> readSymbols(ELFReaderBase &ELFReader);
  Module &m_Module;
};

} // namespace eld

#endif
