#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include <iostream>
#include <string>

using namespace eld::plugin;

class DLL_A_EXPORT ReproducerWithFindConfigFileAbsolutePath
    : public LinkerPlugin {

public:
  ReproducerWithFindConfigFileAbsolutePath()
      : LinkerPlugin("ReproducerWithFindConfigFileAbsolutePath") {}

  // Perform any initialization here
  void Init(const std::string &) override {
    auto expRegisterOption = getLinker()->registerCommandLineOption(
        "--my-config-file", /*hasValue=*/true,
        [&](const std::string &Option,
            const std::optional<std::string> &Value) {
          MyConfigFile = Value.value();
        });
  }

  void ActBeforeRuleMatching() override {
    auto E = getLinker()->findConfigFile(MyConfigFile);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), E);
    std::cout << "Found config file " << E.value() << "\n";

    std::string C = getLinker()->getFileContents(E.value());
    std::cout << "Contents of config file: " << C << "\n";
  }

  std::string MyConfigFile;
};

ELD_REGISTER_PLUGIN(ReproducerWithFindConfigFileAbsolutePath)
