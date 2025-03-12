//===- MappingFileReader.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_MAPPINGFILEREADER_H
#define ELD_SUPPORT_MAPPINGFILEREADER_H

#include "eld/Config/LinkerConfig.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Support/INIReader.h"

namespace eld {

/// \class MappingFileReader
/// \brief Reads a mapping.ini file and ensures it is only read once
class MappingFileReader {
public:
  MappingFileReader(std::string filename) {
    reader = std::make_unique<eld::INIReader>(filename);
  }

  bool readMappingFile(eld::LinkerConfig &pConfig) {
    if (!reader)
      return false;
    auto r = reader->readINIFile();
    if (!r)
      pConfig.raiseDiagEntry(std::move(r.error()));
    if (!r || (r && !r.value()))
      return false;
    for (const std::string &category : reader->getSections())
      for (const auto &p : (*reader)[category].getItems())
        pConfig.addMapping(p.first, p.second);
    return true;
  }

private:
  std::unique_ptr<eld::INIReader> reader;
};
} // namespace eld

#endif
