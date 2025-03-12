//===- ELFExecObjParser.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_ELFEXECOBJPARSER_H
#define ELD_READERS_ELFEXECOBJPARSER_H
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"
namespace eld {
class Module;
class ELFReaderBase;
class ELFExecutableFileReader;

class ELFExecObjParser {
public:
  ELFExecObjParser(Module &module);

  eld::Expected<bool> parseFile(InputFile &inputFile,
                                bool &ELFOverriddenWithBC);

  eld::Expected<bool> parsePatchBase(ELFFileBase &inputFile);

private:
  eld::Expected<void> readSections(ELFReaderBase &ELFReader);
  eld::Expected<bool> readRelocations(InputFile &inputFile);
  Module &m_Module;
};

} // namespace eld
#endif
