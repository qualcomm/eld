#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <algorithm>
#include <iostream>
#include <string>

using namespace eld::plugin;

class DLL_A_EXPORT SortInputSectionsForMergingPlugin : public LinkerPlugin {
public:
  SortInputSectionsForMergingPlugin()
      : LinkerPlugin("SortInputSectionsForMergingPlugin") {}

  void ActBeforeSectionMerging() override {
    auto Before = getLinker()->getInputSectionsForSectionMerging();
    if (!Before) {
      getLinker()->reportDiagEntry(std::move(Before.error()));
      return;
    }
    std::cout << "Before sort:\n";
    for (const auto &S : *Before)
      if (!S.getName().empty())
        std::cout << "  " << S.getName() << "\n";

    auto SortRes = getLinker()->sortInputSectionsForSectionMerging(
        [](const Section &A, const Section &B) {
          return A.getName() > B.getName();
        },
        "DescendingByName");
    if (!SortRes) {
      getLinker()->reportDiagEntry(std::move(SortRes.error()));
      return;
    }

    auto After = getLinker()->getInputSectionsForSectionMerging();
    if (!After) {
      getLinker()->reportDiagEntry(std::move(After.error()));
      return;
    }
    std::cout << "After sort:\n";
    for (const auto &S : *After)
      if (!S.getName().empty())
        std::cout << "  " << S.getName() << "\n";
  }
};

ELD_REGISTER_PLUGIN(SortInputSectionsForMergingPlugin)
