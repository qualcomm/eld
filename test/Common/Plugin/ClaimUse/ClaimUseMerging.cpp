//===- ClaimUseMerging.cpp ------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <iostream>
#include <mutex>
#include <vector>

using namespace eld::plugin;

class ClaimUseMerging : public OutputSectionIteratorPlugin,
                        public LinkerPluginConfig {
public:
  ClaimUseMerging()
      : OutputSectionIteratorPlugin("ClaimUseMerging"),
        LinkerPluginConfig(this) {}
  void Init(std::string) override {}
  void Init() override {
    auto Result = getLinker()->registerReloc(pointerReloc(*getLinker()));
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
  void processOutputSection(OutputSection) override {}
  void RelocCallBack(Use U) override {
    if (U.getSourceChunk().getName() != ".data.claimed")
      return;
    auto Result = getLinker()->claimUse(U);
    if (!Result) {
      getLinker()->reportDiagEntry(std::move(Result.error()));
      return;
    }
    std::lock_guard<std::mutex> Guard(Mutex);
    Uses.push_back({U, U.getSymbol(), U.getTargetChunk(), U.getType(),
                    U.getTargetChunkOffset()});
  }
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    for (const auto &Saved : Uses) {
      Use U = Saved.U;
      if (!(U.getSymbol() == Saved.Target) ||
          !(U.getTargetChunk() == Saved.TargetChunk) ||
          U.getType() != Saved.Type ||
          U.getTargetChunkOffset() != Saved.Addend) {
        getLinker()->getDiagnosticBuilder(getLinker()->getErrorDiagID(
            "claimed merge relocation metadata changed during layout"));
        return Status::ERROR;
      }
      uint64_t Data =
          U.getName() == ".rodata.claim.str" ? 0x12345678 : 0x23456789;
      auto Result = getLinker()->setTargetDataForUse(U, Data);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return Status::ERROR;
      }
    }
    std::cout << "claimed merge uses: " << Uses.size() << "\n";
    return Status::SUCCESS;
  }
  std::string GetName() override { return "ClaimUseMerging"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
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
