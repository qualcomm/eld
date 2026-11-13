//===- ForeignUseWriter.cpp -----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <iostream>
#include <string>
#include <vector>

using namespace eld::plugin;

class ForeignUseWriter : public LinkerPlugin, public LinkerPluginConfig {
public:
  ForeignUseWriter() : LinkerPlugin("ForeignUseWriter"), LinkerPluginConfig() {}
  void Init(const std::string &Options) override { Mode = Options; }
  void Init() override {
    auto Result = getLinker()->registerReloc(pointerReloc(*getLinker()));
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void RelocCallBack(Use U) override {
    if (U.getName() == "data")
      Uses.push_back(U);
  }
  void ActBeforeRelocationScan() override {
    for (Use U : Uses)
      process(U);
  }
  void process(Use U) {
    if (!Mode.empty()) {
      // Deliberately do not report either returned error. The linker must
      // diagnose the conflict itself rather than relying on this plugin.
      if (Mode != "write-only") {
        auto Claim = getLinker()->claimUse(U);
        std::cout << "claimUse: " << (Claim ? "success" : "failure") << "\n";
      }
      auto Write = getLinker()->setTargetDataForUse(
          U, Mode == "same-data" ? 0x87654321 : 0);
      std::cout << "setTargetDataForUse: " << (Write ? "success" : "failure")
                << "\n";
      uint64_t Data = 0;
      auto Read = getLinker()->getTargetDataForUse(U, Data);
      std::cout << "owner replacement retained: "
                << (Read && Data == 0x87654321 ? "yes" : "no") << "\n";
      return;
    }
    auto Result = getLinker()->setTargetDataForUse(U, 0);
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void Destroy() override {}

private:
  std::string Mode;
  std::vector<Use> Uses;
};

ELD_REGISTER_PLUGIN(ForeignUseWriter)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
