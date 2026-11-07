//===- SetTargetDataForUse.cpp --------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <atomic>
#include <thread>
#include <vector>

using namespace eld::plugin;

class SetTargetDataForUse : public OutputSectionIteratorPlugin {
public:
  SetTargetDataForUse() : OutputSectionIteratorPlugin("SetTargetDataForUse") {}
  void Init(std::string) override {}
  void processOutputSection(OutputSection) override {}
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    std::vector<Use> Uses;
    for (InputFile F : getLinker()->getInputFiles())
      for (Section S : F.getSections()) {
        if (S.getName() != ".replacement_data")
          continue;
        auto Result = getLinker()->getUses(S);
        if (!Result) {
          getLinker()->reportDiagEntry(std::move(Result.error()));
          return Status::ERROR;
        }
        Uses.insert(Uses.end(), Result->begin(), Result->end());
      }
    std::atomic<unsigned> Ready{0};
    std::atomic<bool> Start{false};
    std::atomic<bool> Failed{false};
    std::vector<std::thread> Workers;
    constexpr size_t NumWorkers = 8;
    for (size_t W = 0; W < NumWorkers; ++W)
      Workers.emplace_back([&, W] {
        ++Ready;
        while (!Start.load())
          std::this_thread::yield();
        for (size_t I = W; I < Uses.size(); I += NumWorkers) {
          Use U = Uses[I];
          auto Result = getLinker()->setTargetDataForUse(U, 0x12345678);
          if (!Result) {
            Failed = true;
            getLinker()->reportDiagEntry(std::move(Result.error()));
          }
        }
      });
    while (Ready.load() != NumWorkers)
      std::this_thread::yield();
    Start = true;
    for (auto &W : Workers)
      W.join();
    return Failed ? Status::ERROR : Status::SUCCESS;
  }
  std::string GetName() override { return "SetTargetDataForUse"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(SetTargetDataForUse)
