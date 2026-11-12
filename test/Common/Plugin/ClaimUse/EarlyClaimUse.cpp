//===- EarlyClaimUse.cpp --------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <string>
#include <thread>
#include <vector>

using namespace eld::plugin;

// Deliberately has no LinkerPluginConfig and registers no relocation callbacks.
class EarlyClaimUse : public LinkerPlugin {
public:
  EarlyClaimUse() : LinkerPlugin("EarlyClaimUse") {}
  void Init(const std::string &Options) override { Mode = Options; }
  void ActBeforeRuleMatching() override {
    for (InputFile Input : getLinker()->getInputFiles())
      for (Section S : Input.getSections()) {
        auto Result = getLinker()->getUses(S);
        if (!Result) {
          getLinker()->reportDiagEntry(std::move(Result.error()));
          return;
        }
        for (Use U : Result.value())
          if (U.getName() == "data")
            Uses.push_back(U);
      }
    if (Mode == "late-claim")
      return;
    if (Mode == "worker") {
      std::vector<std::thread> Workers;
      for (Use U : Uses)
        Workers.emplace_back([this, U] { claim(U); });
      for (auto &Worker : Workers)
        Worker.join();
    } else {
      for (Use U : Uses)
        claim(U);
    }
  }
  void ActBeforePerformingLayout() override {
    for (Use U : Uses) {
      if (Mode == "late-claim")
        claim(U);
      else if (Mode != "missing")
        write(U);
    }
  }
  void ActBeforeWritingOutput() override {
    if (Mode == "late-data" || Mode == "ignore-late-data")
      for (Use U : Uses)
        write(U);
  }

private:
  void claim(Use U) {
    auto Result = getLinker()->claimUse(U);
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void write(Use U) {
    auto Result = getLinker()->setTargetDataForUse(U, 0x12345678);
    if (!Result && Mode != "ignore-late-data")
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  std::string Mode;
  std::vector<Use> Uses;
};

ELD_REGISTER_PLUGIN(EarlyClaimUse)
