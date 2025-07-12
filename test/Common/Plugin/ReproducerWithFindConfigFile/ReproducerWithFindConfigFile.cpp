#include "Defines.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include <iostream>
#include <string>

using namespace eld::plugin;

class DLL_A_EXPORT ReproducerWithFindConfigFile : public OutputSectionIteratorPlugin {

public:
  ReproducerWithFindConfigFile() : OutputSectionIteratorPlugin("ReproducerWithFindConfigFile") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
     return;
    auto E = getLinker()->findConfigFile(Options);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), E);
    std::cerr << "found config file " << Options << std::endl;
  }

  void processOutputSection(OutputSection O) override { return; }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "ReproducerWithFindConfigFile"; }
};

ELD_REGISTER_PLUGIN(ReproducerWithFindConfigFile)
