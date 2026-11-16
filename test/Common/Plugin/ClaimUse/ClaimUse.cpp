//===- ClaimUse.cpp -------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace eld::plugin;

class ClaimUse : public OutputSectionIteratorPlugin {
public:
  ClaimUse() : OutputSectionIteratorPlugin("ClaimUse") {}
  void Init(std::string Options) override { Mode = Options; }
  void processOutputSection(OutputSection) override {}
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    for (Use U : Uses) {
      auto Result = Mode == "invalid"
                        ? getLinker()->claimUse(U)
                        : getLinker()->setTargetDataForUse(U, 0x12345678);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return Status::ERROR;
      }
    }
    return Status::SUCCESS;
  }
  void collect(Use U) {
    if (U.getName() != "data")
      return;
    std::lock_guard<std::mutex> Guard(Mutex);
    if (CallbackThread == std::thread::id())
      CallbackThread = std::this_thread::get_id();
    else if (CallbackThread != std::this_thread::get_id()) {
      getLinker()->getDiagnosticBuilder(
          getLinker()->getErrorDiagID("relocation callbacks are not serial"));
      return;
    }
    if (Mode == "unclaimed-data" || Mode == "claimed-data") {
      if (Mode == "claimed-data") {
        auto Result = getLinker()->claimUse(U);
        if (!Result) {
          getLinker()->reportDiagEntry(std::move(Result.error()));
          return;
        }
      }
      // A repeated write by the same plugin succeeds and updates its value.
      // Both values intentionally differ from a competing writer's.
      for (uint64_t Data : {0x12345678U, 0x87654321U}) {
        auto Result = getLinker()->setTargetDataForUse(U, Data);
        if (!Result)
          getLinker()->reportDiagEntry(std::move(Result.error()));
      }
      return;
    }
    if (Mode == "other-use") {
      if (Previous) {
        auto Result = getLinker()->claimUse(Previous);
        if (!Result)
          getLinker()->reportDiagEntry(std::move(Result.error()));
        else
          Uses.push_back(Previous);
      }
      Previous = U;
      return;
    }
    if (Mode == "overridden") {
      auto Result = getLinker()->setTargetDataForUse(U, 0);
      if (!Result)
        getLinker()->reportDiagEntry(std::move(Result.error()));
    }
    if (Mode != "invalid") {
      auto Result = getLinker()->claimUse(U);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return;
      }
    }
    if (Mode == "duplicate") {
      auto Result = getLinker()->claimUse(U);
      if (!Result)
        getLinker()->reportDiagEntry(std::move(Result.error()));
    } else if (Mode != "missing") {
      Uses.push_back(U);
    }
  }
  std::string GetName() override { return "ClaimUse"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}

private:
  std::mutex Mutex;
  std::thread::id CallbackThread;
  std::string Mode;
  std::vector<Use> Uses;
  Use Previous{nullptr};
};

ELD_REGISTER_PLUGIN(ClaimUse)

class ClaimConfig : public LinkerPluginConfig {
public:
  ClaimConfig() : LinkerPluginConfig(ThisPlugin) {}
  void Init() override {
    auto *LW = ThisPlugin->getLinker();
    auto Result = LW->registerReloc(pointerReloc(*LW));
    if (!Result)
      LW->reportDiagEntry(std::move(Result.error()));
  }
  void RelocCallBack(Use U) override { ThisPlugin->collect(U); }
};

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  static ClaimConfig Config;
  return &Config;
}
