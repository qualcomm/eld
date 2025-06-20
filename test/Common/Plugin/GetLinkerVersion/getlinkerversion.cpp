#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT GetLinkerVersion : public LinkerPlugin {

public:
  getLinkerVersion() : LinkerPlugin("GetLinkerVersion") {}

  void Init(const std::string &Options) override;

  void Destroy() override {}
};

void GetLinkerVersion::Init(const std::string &Options) {
  std::cout << "ELD Version is: " << getLinker()->getLinkerVersion() << "\n";
}

ELD_REGISTER_PLUGIN(GetLinkerVersion)
