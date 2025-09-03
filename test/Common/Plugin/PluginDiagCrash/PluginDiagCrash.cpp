#include "Defines.h"
#include "LinkerPlugin.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT PluginDiagCrash : public eld::plugin::LinkerPlugin {
public:
  PluginDiagCrash() : eld::plugin::LinkerPlugin("PluginDiagCrash") {}
  void Init(const std::string &options) override {
    auto diagID = getLinker()->getNoteDiagID("A note diagnostic: %0 %1");
    getLinker()->reportDiag(diagID, "A", "Long", "List", "Of", "Too", "Many",
                            "Arguments", "That", "Exceeds", "The", "Max",
                            "Number", "Of", "Supported", "Arguments");
  };
};

ELD_REGISTER_PLUGIN(PluginDiagCrash)
