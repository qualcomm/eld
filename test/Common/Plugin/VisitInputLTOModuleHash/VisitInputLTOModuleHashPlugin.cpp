#include "Defines.h"
#include "Diagnostics.h"
#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT VisitInputLTOModuleHashPlugin
    : public eld::plugin::LinkerPlugin {
public:
  VisitInputLTOModuleHashPlugin()
      : eld::plugin::LinkerPlugin("VisitInputLTOModuleHashPlugin") {}

  void VisitInput(eld::plugin::InputFile IF) override {
    std::string path = IF.decoratedPath();
    auto BF = IF.getAsBitcodeFile();
    if (!BF)
      return;

    SavedBitcode = BF;
    if (path.find("set_hash") != std::string::npos) {
      auto Exp = BF.setLTOModuleHash(0x12345678ULL, *getLinker());
      if (!Exp) {
        getLinker()->reportDiagEntry(std::move(Exp.error()));
      } else {
        auto H = BF.getLTOModuleHash();
        std::cout << "VisitInput: bitcode hash set=0x" << std::hex
                  << (H.has_value() ? *H : 0) << std::dec << " for '"
                  << BF.getFileName() << "'\n";
      }
    }
    if (path.find("dup_hash") != std::string::npos) {
      auto Exp1 = BF.setLTOModuleHash(0x1111ULL, *getLinker());
      if (!Exp1)
        getLinker()->reportDiagEntry(std::move(Exp1.error()));
      auto Exp2 = BF.setLTOModuleHash(0x2222ULL, *getLinker());
      if (!Exp2)
        getLinker()->reportDiagEntry(std::move(Exp2.error()));
    }
  }

  void ActBeforeRuleMatching() override {
    if (!SavedBitcode ||
        SavedBitcode.getFileName().find("wrong_state") == std::string::npos)
      return;
    auto Exp = SavedBitcode.setLTOModuleHash(0x12345678ULL, *getLinker());
    if (!Exp)
      getLinker()->reportDiagEntry(std::move(Exp.error()));
  }

private:
  eld::plugin::BitcodeFile SavedBitcode{nullptr};
};

ELD_REGISTER_PLUGIN(VisitInputLTOModuleHashPlugin)
