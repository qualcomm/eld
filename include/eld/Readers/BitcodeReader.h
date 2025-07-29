//===- BitcodeReader.h-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_BITCODEREADER_H
#define ELD_READERS_BITCODEREADER_H

namespace eld::plugin {
class LinkerPlugin;
}

namespace eld {

class Module;
class InputFile;

class BitcodeReader {

public:
  BitcodeReader(Module &);
  ~BitcodeReader();

  bool readInput(InputFile &pFile, plugin::LinkerPlugin *LTOPlugin);

private:
  Module &m_Module;
  bool m_TraceLTO;
};

} // namespace eld

#endif
