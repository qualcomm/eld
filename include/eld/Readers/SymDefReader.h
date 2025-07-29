//===- SymDefReader.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_READERS_SYMDEFREADER_H
#define ELD_READERS_SYMDEFREADER_H

#include "eld/Readers/ObjectReader.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include <tuple>

namespace eld {

class InputFile;
class Module;

class SymDefReader : public ObjectReader {

public:
  SymDefReader(Module &);
  ~SymDefReader();

  /* read symdef header*/
  bool readHeader(InputFile &pFile, bool isPostLTOPhase = false) override;
  bool readSymbols(InputFile &pFile, bool isPostLTOPhase = false) override;
  bool readRelocations(InputFile &pFile) override;
  bool readSections(InputFile &pFile, bool isPostLTOPhase = false) override;
  void processComment(llvm::StringRef line);
  void getSymDefStyle(llvm::StringRef line);

private:
  std::vector<std::tuple<uint64_t, ResolveInfo::Type, std::string>>
  readSymDefs(InputFile &pFile);

private:
  Module &m_Module;
};

} // namespace eld

#endif
