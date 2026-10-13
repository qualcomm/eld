#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerScript.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetLinkerScriptHash : public LinkerPlugin {
public:
  GetLinkerScriptHash() : LinkerPlugin("GetLinkerScriptHash") {}

  void Init(const std::string &Options) override {}

  void ActBeforeSectionMerging() override {
    std::cout << "linker script hash: "
              << getLinker()->getLinkerScript().getHash() << "\n";
  }

  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(GetLinkerScriptHash)
