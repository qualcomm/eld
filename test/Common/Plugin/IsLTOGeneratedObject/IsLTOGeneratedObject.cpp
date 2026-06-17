#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT IsLTOGeneratedObjectPlugin : public LinkerPlugin {
public:
  IsLTOGeneratedObjectPlugin() : LinkerPlugin("IsLTOGeneratedObjectPlugin") {}

  void ActBeforeSectionMerging() override {
    for (auto &I : getLinker()->getInputFiles()) {
      if (!I.isObjectFile())
        continue;
      std::cout << I.getFileName()
                << " isLTOGeneratedObject: " << I.isLTOGeneratedObject()
                << "\n";
    }
  }
};

ELD_REGISTER_PLUGIN(IsLTOGeneratedObjectPlugin)
