//===- SymbolPreemptibility.cpp -------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class SymbolPreemptibility : public OutputSectionIteratorPlugin {
public:
  SymbolPreemptibility()
      : OutputSectionIteratorPlugin("SymbolPreemptibility") {}
  void Init(std::string) override {}
  void processOutputSection(OutputSection) override {}
  Status Run(bool) override {
    if (!getLinker()->isLinkStateAfterLayout())
      return Status::SUCCESS;
    std::cout << "null=" << getLinker()->isSymbolPreemptible(Symbol(nullptr))
              << "\n";
    for (InputFile F : getLinker()->getInputFiles())
      for (Symbol S : F.getSymbols())
        if (S.getName().find("preempt_") == 0)
          std::cout << S.getName() << "=" << getLinker()->isSymbolPreemptible(S)
                    << "\n";
    return Status::SUCCESS;
  }
  std::string GetName() override { return "SymbolPreemptibility"; }
  std::string GetLastErrorAsString() override { return ""; }
  uint32_t GetLastError() override { return 0; }
  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(SymbolPreemptibility)
