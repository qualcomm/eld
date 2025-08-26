#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class OutSectionsBeforePerformingLayout : public eld::plugin::LinkerPlugin {
public:
  OutSectionsBeforePerformingLayout()
      : eld::plugin::LinkerPlugin("OutSectionsBeforePerformingLayout") {}
  void ActBeforePerformingLayout() override {
    auto expOutSects = getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expOutSects);
    std::vector<eld::plugin::OutputSection> outSects =
        std::move(expOutSects.value());
    for (auto O : outSects) {
      std::cout << O.getIndex() << "\n";
    }
  }
};


ELD_REGISTER_PLUGIN(OutSectionsBeforePerformingLayout)