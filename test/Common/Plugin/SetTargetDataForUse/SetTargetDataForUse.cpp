//===- SetTargetDataForUse.cpp --------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#ifdef BEFORE_WRITING_OUTPUT
#include "LinkerPlugin.h"
#else
#include "OutputSectionIteratorPlugin.h"
#endif
#include "PluginVersion.h"
#include <atomic>
#include <thread>
#include <vector>

using namespace eld::plugin;

#ifdef BEFORE_WRITING_OUTPUT
using WriterPlugin = LinkerPlugin;
#else
using WriterPlugin = OutputSectionIteratorPlugin;
#endif

class SetTargetDataForUse : public WriterPlugin {
public:
  SetTargetDataForUse() : WriterPlugin("SetTargetDataForUse") {}
#ifdef BEFORE_WRITING_OUTPUT
  void Init(const std::string &Options) override {
    ClaimUses = Options == "claimed";
  }
  void ActBeforeRelocationScan() override {
    if (!ClaimUses)
      return;
    std::vector<Use> Uses;
    if (!collectUses(Uses))
      return;
    for (Use U : Uses) {
      auto Result = getLinker()->claimUse(U);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return;
      }
    }
  }
  void ActBeforeWritingOutput() override { writeUses(); }
#else
  void Init(std::string) override {}
  void processOutputSection(OutputSection) override {}
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    return writeUses() ? Status::SUCCESS : Status::ERROR;
  }
  std::string GetName() override { return "SetTargetDataForUse"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
#endif
  void Destroy() override {}

private:
  bool collectUses(std::vector<Use> &Uses) {
    for (InputFile F : getLinker()->getInputFiles())
      for (Section S : F.getSections()) {
        if (S.getName() != ".replacement_data")
          continue;
        auto Result = getLinker()->getUses(S);
        if (!Result) {
          getLinker()->reportDiagEntry(std::move(Result.error()));
          return false;
        }
        Uses.insert(Uses.end(), Result->begin(), Result->end());
      }
    return true;
  }
  bool writeUses() {
    std::vector<Use> Uses;
    if (!collectUses(Uses))
      return false;
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
    return !Failed;
  }
#ifdef BEFORE_WRITING_OUTPUT
  bool ClaimUses = false;
#endif
};

ELD_REGISTER_PLUGIN(SetTargetDataForUse)
