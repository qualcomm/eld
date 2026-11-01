//===- EarlyClaimUse.cpp --------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>
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
    if (Mode != "early-claim")
      return;
    collectUses();
    for (Use U : Uses)
      claim(U);
  }
  void ActBeforeRelocationScan() override {
    std::cout << "scan hook: " << getLinker()->getCurrentLinkStateAsStr()
              << " before-layout=" << getLinker()->isLinkStateBeforeLayout()
              << "\n";
    collectUses();
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
  void collectUses() {
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
  }
  void ActBeforePerformingLayout() override {
    for (Use U : Uses) {
      if (Mode == "late-claim")
        claim(U);
      else if (Mode != "missing" && Mode != "output-data")
        write(U);
    }
  }
  void ActBeforeWritingOutput() override {
    if (Mode == "late-data" || Mode == "output-data")
      for (Use U : Uses)
        write(U, 0x87654321);
  }
  void Destroy() override {
    if (Mode == "after-output" || Mode == "ignore-after-output")
      for (Use U : Uses)
        write(U, 0xaaaaaaaa);
  }

private:
  void claim(Use U) {
    auto Result = getLinker()->claimUse(U);
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void write(Use U, uint64_t Data = 0x12345678) {
    auto Result = getLinker()->setTargetDataForUse(U, Data);
    if (!Result && Mode != "ignore-after-output")
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  std::string Mode;
  std::vector<Use> Uses;
};

ELD_REGISTER_PLUGIN(EarlyClaimUse)
