//===- HexagonClaimUse.cpp -----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>
#include <mutex>
#include <vector>

using namespace eld::plugin;

class HexagonClaimUse : public OutputSectionIteratorPlugin,
                        public LinkerPluginConfig {
public:
  HexagonClaimUse()
      : OutputSectionIteratorPlugin("HexagonClaimUse"),
        LinkerPluginConfig(this) {}
  void Init(std::string) override {}
  void Init() override {
    for (uint32_t Type : {
             1,  // R_HEX_B22_PCREL
             36, // R_HEX_PLT_B22_PCREL
             69, // R_HEX_GOT_32_6_X
             71  // R_HEX_GOT_11_X
         }) {
      auto Result = getLinker()->registerReloc(Type);
      if (!Result)
        getLinker()->reportDiagEntry(std::move(Result.error()));
    }
  }
  void processOutputSection(OutputSection) override {}
  void RelocCallBack(Use U) override {
    if (U.getName().find("claimed_") != 0)
      return;
    auto Result = getLinker()->claimUse(U);
    if (!Result) {
      getLinker()->reportDiagEntry(std::move(Result.error()));
      return;
    }
    std::lock_guard<std::mutex> Guard(Mutex);
    Uses.push_back({U, U.getSymbol(), U.getType(), U.getTargetChunkOffset()});
  }
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    for (const auto &Saved : Uses) {
      Use U = Saved.U;
      if (!(U.getSymbol() == Saved.Target) || U.getType() != Saved.Type ||
          U.getTargetChunkOffset() != Saved.Addend) {
        getLinker()->getDiagnosticBuilder(getLinker()->getErrorDiagID(
            "claimed Hexagon relocation metadata changed during layout"));
        return Status::ERROR;
      }
      // A complete nop instruction replaces each claimed operand.
      auto Result = getLinker()->setTargetDataForUse(U, 0x7f00c000);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return Status::ERROR;
      }
    }
    std::cout << "claimed uses: " << Uses.size() << "\n";
    return Status::SUCCESS;
  }
  std::string GetName() override { return "HexagonClaimUse"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}

private:
  struct SavedUse {
    Use U;
    Symbol Target;
    uint32_t Type;
    int64_t Addend;
  };
  std::mutex Mutex;
  std::vector<SavedUse> Uses;
};

ELD_REGISTER_PLUGIN(HexagonClaimUse)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
