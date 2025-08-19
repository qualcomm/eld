#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT InputFilePluginAPI : public LinkerPlugin {

public:
  InputFilePluginAPI() : LinkerPlugin("InputFilePluginAPI") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override {
    auto InputFiles = getLinker()->getInputFiles();
    for (auto &I : InputFiles) {
      if (I.isObjectFile()) {
        std::cout << " \n The hash of :" << I.getFileName()
                  << " is: " << I.getResolvedPathHash() << "\n";
      }
      if (I.isArchive()) {
        std::cout << "The hash of archive member " << I.getMemberName()
                  << " is: " << I.getArchiveMemberNameHash() << "\n";
      }
    }
  }
  void Destroy() override {}
};

void InputFilePluginAPI::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(InputFilePluginAPI)