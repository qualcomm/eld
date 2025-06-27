#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT InputSectionAPIs : public LinkerPlugin {

public:
  InputSectionAPIs() : LinkerPlugin("InputSectionAPIs") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override {
    for (auto IF : getLinker()->getInputFiles()) {
      for (auto S : IF.getSections()) {
        if (S.isIgnore()) {
          std::cout << "Section: " << S.getName() << " is of Ignore kind."
                    << "\n";
        }
        if (S.isRelocation())
          std::cout << "Section: " << S.getName() << " is of Relocation kind."
                    << "\n";
        if (S.isGroup())
          std::cout << "Section: " << S.getName() << " is of group kind."
                    << "\n";
      }
    }
  }
  void Destroy() override {}
};

void InputSectionAPIs::Init(const std::string &Options) {}

ELD_REGISTER_PLUGIN(InputSectionAPIs)