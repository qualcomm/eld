//===- BitcodeFile.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_BITCODEFILE_H
#define ELD_INPUT_BITCODEFILE_H

#include "eld/Input/ObjectFile.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/LTO/LTO.h"

#include "eld/PluginAPI/Expected.h"

#include <memory>

namespace llvm {
namespace lto {
class QCInputFileAdapter;
} // namespace lto
} // namespace llvm

namespace eld::plugin {
class LinkerPlugin;
class LTOModule;
} // namespace eld::plugin

namespace eld {

class DiagnosticEngine;

/** \class InputFile
 *  \brief InputFile represents a real object file, a linker script or anything
 *  that the rest of the linker can work with.
 */
class BitcodeFile : public ObjectFile {
public:
  BitcodeFile(Input *I, DiagnosticEngine *DiagEngine);

  ~BitcodeFile();

  /// Casting support.
  static bool classof(const InputFile *I) {
    return (I->getKind() == InputFile::BitcodeFileKind);
  }

  bool createLTOInputFile(const std::string &ModuleID);

  llvm::lto::InputFile &getInputFile() const { return *LTOInputFile; }

  std::unique_ptr<llvm::lto::InputFile> takeLTOInputFile();

  /// Release Memory
  bool canReleaseMemory() const;

  void releaseMemory(bool IsVerbose = false);

  // --------- Comdat Table -------------------------
  bool findIfKeptComdat(int Index) const {
    if (Index == -1) /* Not a part of group */
      return true;
    const auto &It = BCComdats.find(Index);
    if (It != BCComdats.end() && It->second == false)
      return false;
    return true;
  }

  void addKeptComdat(int Index, bool Kept) { BCComdats[Index] = Kept; }

  bool createPluginModule(plugin::LinkerPlugin &, uint64_t Hash);

  // TODO: Used by BitcodeReader, may not be needed.
  // TODO: class reference.
  plugin::LTOModule *getPluginModule() { return PluginModule; }

  void createBitcodeFilePlugin(plugin::LinkerPlugin &LTOPlugin);

  void setInputSectionForSymbol(const ResolveInfo &, Section &);

  Section *getInputSectionForSymbol(const ResolveInfo &) const;

private:
  DiagnosticEngine *DiagEngine = nullptr;
  std::string ModuleID;
  std::unique_ptr<llvm::lto::InputFile> LTOInputFile;
  // Marked by comdat index in Module if accepted: (true if not rejected)
  llvm::DenseMap<int, bool> BCComdats;
  std::unordered_map<const ResolveInfo *, Section *> InputSectionForSymbol;
  plugin::LTOModule *PluginModule;
};

} // namespace eld

#endif // ELD_INPUT_BITCODEFILE_H
