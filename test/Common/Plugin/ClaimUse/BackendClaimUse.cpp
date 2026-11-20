//===- BackendClaimUse.cpp -----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "RISCVRelocationInternal.h"
#include "llvm/BinaryFormat/ELF.h"
#include <iostream>
#include <vector>

using namespace eld::plugin;

class BackendClaimUse : public LinkerPlugin, public LinkerPluginConfig {
public:
  BackendClaimUse() : LinkerPlugin("BackendClaimUse") {}
  void Init(const std::string &Options) override { Architecture = Options; }
  void Init() override {
    using namespace llvm::ELF;
    std::vector<uint32_t> Types;
    if (Architecture == "riscv")
      Types = {R_RISCV_CALL, R_RISCV_CALL_PLT, R_RISCV_JAL};
    else if (Architecture == "riscv-got-lo")
      Types = {R_RISCV_PCREL_LO12_I};
    else if (Architecture == "riscv-qc-access")
      Types = {eld::ELF::riscv::internal::R_RISCV_QC_ACCESS_16,
               eld::ELF::riscv::internal::R_RISCV_QC_ACCESS_32};
    else if (Architecture == "arm")
      Types = {R_ARM_CALL, R_ARM_JUMP24, R_ARM_THM_CALL, R_ARM_THM_JUMP24};
    else if (Architecture == "aarch64")
      Types = {R_AARCH64_CALL26, R_AARCH64_JUMP26,
               R_AARCH64_LDST64_ABS_LO12_NC};
    for (uint32_t Type : Types) {
      auto Result = getLinker()->registerReloc(Type);
      if (!Result)
        getLinker()->reportDiagEntry(std::move(Result.error()));
    }
  }
  void RelocCallBack(Use U) override {
    if (U.getName().find("claimed_") == 0)
      Uses.push_back({U, U.getSymbol(), U.getSourceChunk(), U.getType(),
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
    using namespace llvm::ELF;
    for (const auto &Saved : Uses) {
      Use U = Saved.U;
      // Earlier unclaimed instructions may relax and shift the source offset.
      if (!(U.getSymbol() == Saved.Target) ||
          !(U.getSourceChunk() == Saved.Source) || U.getType() != Saved.Type ||
          U.getTargetChunkOffset() != Saved.Addend) {
        getLinker()->getDiagnosticBuilder(getLinker()->getErrorDiagID(
            "claimed relocation metadata changed during layout"));
        return;
      }
      uint64_t Data;
      if (Architecture.find("riscv") == 0)
        Data = U.getType() == eld::ELF::riscv::internal::R_RISCV_QC_ACCESS_16
                   ? 0x0001
                   : 0x00000013;
      else if (Architecture == "arm")
        Data = U.getType() == R_ARM_THM_CALL || U.getType() == R_ARM_THM_JUMP24
                   ? 0xbf00bf00
                   : 0xe1a00000;
      else
        Data = 0xd503201f;
      auto Result = getLinker()->setTargetDataForUse(U, Data);
      if (!Result) {
        getLinker()->reportDiagEntry(std::move(Result.error()));
        return;
      }
    }
    std::cout << "claimed uses: " << Uses.size() << "\n";
  }
  void Destroy() override {}

private:
  struct SavedUse {
    Use U;
    Symbol Target;
    Chunk Source;
    uint32_t Type;
    int64_t Addend;
  };
  std::string Architecture;
  std::vector<SavedUse> Uses;
};

ELD_REGISTER_PLUGIN(BackendClaimUse)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
