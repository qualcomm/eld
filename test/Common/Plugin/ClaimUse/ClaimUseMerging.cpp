//===- ClaimUseMerging.cpp ------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <iostream>
#include <mutex>
#include <vector>

using namespace eld::plugin;

class ClaimUseMerging : public LinkerPlugin, public LinkerPluginConfig {
public:
  ClaimUseMerging() : LinkerPlugin("ClaimUseMerging"), LinkerPluginConfig() {}
  void Init(const std::string &) override {}
  void Init() override {
    auto Result = getLinker()->registerReloc(pointerReloc(*getLinker()));
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void RelocCallBack(Use U) override {
    if (U.getSourceChunk().getName() != ".data.claimed")
      return;
    std::lock_guard<std::mutex> Guard(Mutex);
    Uses.push_back({U, U.getSymbol(), U.getTargetChunk(), U.getType(),
                    U.getTargetChunkOffset()});
  }
  void ActBeforeRelocationScan() override {
    for (const auto &Saved : Uses) {
      auto Result = getLinker()->claimUse(Saved.U);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return;
      }
    }
  }
  void ActBeforeWritingOutput() override {
    for (const auto &Saved : Uses) {
      Use U = Saved.U;
      if (!(U.getSymbol() == Saved.Target) ||
          !(U.getTargetChunk() == Saved.TargetChunk) ||
          U.getType() != Saved.Type ||
          U.getTargetChunkOffset() != Saved.Addend) {
        getLinker()->getDiagnosticBuilder(getLinker()->getErrorDiagID(
            "claimed merge relocation metadata changed during layout"));
        return;
      }
      uint64_t Data =
          U.getName() == ".rodata.claim.str" ? 0x12345678 : 0x23456789;
      auto Result = getLinker()->setTargetDataForUse(U, Data);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return;
      }
    }
    std::cout << "claimed merge uses: " << Uses.size() << "\n";
  }
  void Destroy() override {}

private:
  struct SavedUse {
    Use U;
    Symbol Target;
    Chunk TargetChunk;
    uint32_t Type;
    int64_t Addend;
  };
  std::mutex Mutex;
  std::vector<SavedUse> Uses;
};

ELD_REGISTER_PLUGIN(ClaimUseMerging)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
