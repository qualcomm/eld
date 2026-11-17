//===- DWARFInfoRetry.cpp -------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "DWARF.h"
#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <cstddef>
#include <iostream>
#include <vector>

using namespace eld::plugin;

class DLL_A_EXPORT DWARFInfoRetryPlugin : public LinkerPlugin {
public:
  DWARFInfoRetryPlugin() : LinkerPlugin("DWARFInfoRetry") {}

  void VisitInput(InputFile IF) override { checkDWARF(IF, "VisitInput"); }

  void VisitSections(InputFile IF) override {
    if (IF.getFileName().find("late.o") == std::string::npos) {
      checkDWARF(IF, "VisitSections");
      eld::Expected<void> DeleteResult =
          getLinker()->deleteDWARFInfoForInputFile(IF);
      std::cout << "DWARF: VisitSections deleted=" << (DeleteResult ? 1 : 0)
                << '\n';
    }
    InputFiles.push_back(IF);
  }

  void ActBeforePerformingLayout() override {
    for (InputFile IF : InputFiles)
      checkDWARF(IF, "ActBeforePerformingLayout");
  }

private:
  std::vector<InputFile> InputFiles;

  void checkDWARF(InputFile IF, const char *Hook) {
    eld::Expected<DWARFInfo> Info =
        getLinker()->getDWARFInfoForInputFile(IF, true);
    std::size_t UnitCount = 0;
    std::size_t SubprogramCount = 0;
    if (Info && Info->hasDWARFContext()) {
      std::vector<DWARFUnit> Units = Info->getDWARFUnits();
      UnitCount = Units.size();
      for (DWARFUnit &Unit : Units)
        for (DWARFDie &Die : Unit.getDIEs())
          SubprogramCount += Die.isSubprogramDIE();
    }
    std::cout << "DWARF: " << Hook
              << " hasContext=" << (Info && Info->hasDWARFContext() ? 1 : 0)
              << " units=" << UnitCount << " subprograms=" << SubprogramCount
              << " input=" << IF.getFileName() << '\n';
  }
};

ELD_REGISTER_PLUGIN(DWARFInfoRetryPlugin)
