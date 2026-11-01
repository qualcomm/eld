//===- ForeignUseWriter.cpp -----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <iostream>
#include <string>

using namespace eld::plugin;

class ForeignUseWriter : public OutputSectionIteratorPlugin,
                         public LinkerPluginConfig {
public:
  ForeignUseWriter()
      : OutputSectionIteratorPlugin("ForeignUseWriter"),
        LinkerPluginConfig(this) {}
  void Init(std::string Options) override { Mode = Options; }
  void Init() override {
    auto Result = getLinker()->registerReloc(pointerReloc(*getLinker()));
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void processOutputSection(OutputSection) override {}
  Status Run(bool) override { return Status::SUCCESS; }
  void RelocCallBack(Use U) override {
    if (U.getName() != "data")
      return;
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
  std::string GetName() override { return "ForeignUseWriter"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}

private:
  std::string Mode;
};

ELD_REGISTER_PLUGIN(ForeignUseWriter)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
