#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetOutputSectionHash : public LinkerPlugin {

public:
  GetOutputSectionHash() : LinkerPlugin("GetOutputSectionHash") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override {
    auto O = getLinker()->getOutputSection(".foo");
    std::cout << "The output section hash of " << O->getName()
              << " is: " << O->getHash() << "\n";
  }
  void Destroy() override {}
};

void GetOutputSectionHash::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(GetOutputSectionHash)
