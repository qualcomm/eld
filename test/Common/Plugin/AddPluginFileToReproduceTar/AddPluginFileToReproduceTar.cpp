#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class AddPluginFileToReproduceTar : public eld::plugin::LinkerPlugin {
public:
  AddPluginFileToReproduceTar()
      : eld::plugin::LinkerPlugin("AddPluginFileToReproduceTar") {}
  void Init(const std::string &Options) override {
    auto expRegisterOption = getLinker()->registerCommandLineOption(
        "--plugin-file", /*hasValue=*/true,
        [&](const std::string &Option,
            const std::optional<std::string> &Value) {
          PluginFile = Value.value();
        });
  }
  void ActBeforeRuleMatching() override {
    auto E = getLinker()->addFileToReproduceTar(PluginFile);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), E);
  }
  void Destroy() override {}

  std::string PluginFile;
};

ELD_REGISTER_PLUGIN(AddPluginFileToReproduceTar)