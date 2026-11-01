//===- ClaimUse.cpp -------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace eld::plugin;

class ClaimUse : public LinkerPlugin {
public:
  ClaimUse() : LinkerPlugin("ClaimUse") {}
  void Init(const std::string &Options) override { Mode = Options; }
  void collect(Use U) {
    if (U.getName() != "data")
      return;
    if (CallbackThread == std::thread::id())
      CallbackThread = std::this_thread::get_id();
    else if (CallbackThread != std::this_thread::get_id()) {
      getLinker()->getDiagnosticBuilder(
          getLinker()->getErrorDiagID("relocation callbacks are not serial"));
      return;
    }
    // A saved use can be claimed later, independently of the current callback.
    if (Mode == "other-use") {
      if (Previous)
        Uses.push_back(Previous);
      Previous = U;
    } else {
      Uses.push_back(U);
    }
  }
  void ActBeforeRelocationScan() override {
    if (Mode == "invalid" || Mode == "late-before-layout")
      return;
    for (Use U : Uses) {
      if (Mode == "overridden")
        write(U, 0);
      if (Mode != "unclaimed-data") {
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
      }
      if (Mode == "unclaimed-data" || Mode == "claimed-data") {
        write(U, 0x12345678);
        write(U, 0x87654321);
      }
    }
  }
  void ActBeforeWritingOutput() override {
    if (Mode == "missing" || Mode == "late-before-layout" ||
        Mode == "unclaimed-data" || Mode == "claimed-data")
      return;
    for (Use U : Uses) {
      if (Mode == "invalid") {
        auto Result = getLinker()->claimUse(U);
        if (!Result)
          getLinker()->reportDiagEntry(std::move(Result.error()));
      } else {
        write(U, 0x12345678);
      }
    }
  }

private:
  void write(Use U, uint64_t Data) {
    auto Result = getLinker()->setTargetDataForUse(U, Data);
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  std::thread::id CallbackThread;
  std::string Mode;
  std::vector<Use> Uses;
  Use Previous{nullptr};
};

ELD_REGISTER_PLUGIN(ClaimUse)

class ClaimConfig : public LinkerPluginConfig {
public:
  ClaimConfig() : LinkerPluginConfig() {}
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
