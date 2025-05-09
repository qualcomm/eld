//===- YamlLayoutPrinter.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_LAYOUTMAP_YAMLLAYOUTPRINTER_H
#define ELD_LAYOUTMAP_YAMLLAYOUTPRINTER_H

#include "eld/LayoutMap/LDYAML.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MemoryRegion.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {
class LinkerConfig;
struct CommandLineDefault {
  std::string Name;
  std::string Value;
  llvm::StringRef Desc;
};

class YamlLayoutPrinter {

public:
  YamlLayoutPrinter(LayoutInfo *layoutInfo);

  eld::Expected<void> init();

  llvm::raw_ostream &outputStream() const;

  eld::LDYAML::Module buildYaml(eld::Module &Module,
                                GNULDBackend const &Backend);

  void printLayout(eld::Module &Module, GNULDBackend const &Backend);

  ~YamlLayoutPrinter() {
    delete LayoutFile;
    delete TrampolineLayoutFile;
  }

  uint64_t getEntryAddress(eld::Module const &CurModule,
                           GNULDBackend const &Backend);

  void addInputs(std::vector<std::shared_ptr<eld::LDYAML::InputFile>> &Inputs,
                 eld::Module &Module);

  eld::LDYAML::LinkStats addStat(std::string S, uint64_t Count);

  void addStats(LayoutInfo::Stats &L,
                std::vector<eld::LDYAML::LinkStats> &S);

  void insertCommons(std::vector<eld::LDYAML::Common> &Commons,
                     const std::vector<ResolveInfo *> &Infos);

  void getTrampolineMap(eld::Module &Module,
                        std::vector<eld::LDYAML::TrampolineInfo> &R);

private:
  std::vector<CommandLineDefault> Defaults;
  std::string CommandLine;
  llvm::raw_fd_ostream *LayoutFile;
  llvm::raw_fd_ostream *TrampolineLayoutFile;
  LayoutInfo *ThisLayoutInfo = nullptr;
};

} // namespace eld

#endif
