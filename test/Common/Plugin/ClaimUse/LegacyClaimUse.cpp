//===- LegacyClaimUse.cpp
//--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include "PointerReloc.h"
#include "SectionIteratorPlugin.h"
#include <iostream>
#include <string>
#include <vector>

using namespace eld::plugin;

#ifdef SECTION_ITERATOR
using LegacyBase = SectionIteratorPlugin;
#else
using LegacyBase = OutputSectionIteratorPlugin;
#endif

class LegacyClaimUse : public LegacyBase, public LinkerPluginConfig {
public:
  LegacyClaimUse() : LegacyBase("LegacyClaimUse"), LinkerPluginConfig(this) {}
  void Init(std::string Options) override { Mode = Options; }
  void Init() override {
    auto Result = getLinker()->registerReloc(pointerReloc(*getLinker()));
    if (!Result)
      getLinker()->reportDiagEntry(std::move(Result.error()));
  }
#ifdef SECTION_ITERATOR
  void processSection(Section) override {}
#else
  void processOutputSection(OutputSection) override {}
#endif
  void RelocCallBack(Use U) override {
    if (U.getName() != "data")
      return;
    observe("relocation callback");
    if (Mode == "callback-claim")
      claim(U);
    Uses.push_back(U);
  }
  Status Run(bool) override {
    if (getLinker()->isLinkStateBeforeLayout()) {
      observe("iterator callback");
      if (Mode == "claim")
        for (Use U : Uses)
          claim(U);
    }
    if (getLinker()->isLinkStateCreatingSegments() &&
        (Mode == "do-relocation" || Mode == "apply-relocations")) {
      std::vector<uint8_t> Buffer(65536);
      auto Result = Mode == "do-relocation"
                        ? getLinker()->doRelocation()
                        : getLinker()->applyRelocations(Buffer.data());
      if (!Result)
        std::cerr << "buffer API rejected\n";
    }
    return Status::SUCCESS;
  }
  std::string GetName() override { return "LegacyClaimUse"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}

private:
  void observe(const char *Callback) {
    std::cout << Callback << ": " << getLinker()->getCurrentLinkStateAsStr()
              << " before-layout=" << getLinker()->isLinkStateBeforeLayout()
              << "\n";
  }
  void claim(Use U) {
    auto Result = getLinker()->claimUse(U);
    if (!Result)
      std::cerr << "BeforeLayout claim rejected\n";
  }
  std::string Mode;
  std::vector<Use> Uses;
};

ELD_REGISTER_PLUGIN(LegacyClaimUse)

extern "C" LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return ThisPlugin;
}
