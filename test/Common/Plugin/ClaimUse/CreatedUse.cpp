//===- CreatedUse.cpp -----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include <string>
#include <vector>

using namespace eld::plugin;

class CreatedUse : public LinkerPlugin {
public:
  CreatedUse() : LinkerPlugin("CreatedUse") {}
  void Init(const std::string &Options) override { Mode = Options; }
  void ActBeforeRuleMatching() override {
    if (!Chunks.empty())
      return;
    auto Target = getLinker()->getSymbol("data");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Target);
    static const char Bytes[] = {0x11, 0x22, 0x33, 0x44,
                                 0x55, 0x66, 0x77, 0x11};
    unsigned Size = getLinker()->is64Bits() ? 8 : 4;
    for (bool Code : {false, true}) {
      auto C = Mode == "nobits"
                   ? getLinker()->createBSSChunk("created_bss", Size, Size)
               : Code ? getLinker()->createChunkWithCustomName(
                            ".created_code", /*SHT_PROGBITS=*/1,
                            /*SHF_ALLOC | SHF_EXECINSTR=*/6, Size, Bytes, Size)
                      : getLinker()->createDataChunkWithCustomName(
                            ".created_data", Size, Bytes, Size);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), C);
      Chunks.push_back(*C);
      // Reject fields that start before the chunk or extend past its end,
      // as well as relocations with no storage bytes.
      off_t Offset = Mode == "negative-offset" ? -1
                     : Mode == "out-of-bounds" ? 1
                                               : 0;
      uint32_t RelocType =
          Mode == "zero-width" ? 0 : pointerReloc(*getLinker());
      auto Result =
          getLinker()->createAndAddUse(*C, Offset, RelocType, *Target, 0);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Result);
      Use U = *Result;
      Uses.push_back(U);
    }
  }
  void ActBeforeRelocationScan() override {
    for (Use U : Uses) {
      if (Mode != "unclaimed") {
        auto Claim = getLinker()->claimUse(U);
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Claim);
      }
      if (Mode != "missing" && Mode != "output-data" && Mode != "output-zero") {
        auto Write = getLinker()->setTargetDataForUse(
            U, Mode == "zero" ? 0 : 0x12345678);
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Write);
      }
    }
  }
  void ActBeforePerformingLayout() override {
    for (Chunk C : Chunks) {
      auto Added = getLinker()->addChunkToOutput(C);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Added);
    }
  }
  void ActBeforeWritingOutput() override {
    if (Mode != "output-data" && Mode != "output-zero")
      return;
    for (Use U : Uses) {
      auto Write = getLinker()->setTargetDataForUse(
          U, Mode == "output-zero" ? 0 : 0x12345678);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Write);
    }
  }

private:
  std::string Mode;
  std::vector<Chunk> Chunks;
  std::vector<Use> Uses;
};

ELD_REGISTER_PLUGIN(CreatedUse)
