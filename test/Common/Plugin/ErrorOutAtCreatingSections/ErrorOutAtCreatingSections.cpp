#include "Defines.h"
#include "PluginBase.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <cassert>
#include <map>

class DLL_A_EXPORT ErrorOutAtCreatingSections
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  ErrorOutAtCreatingSections()
      : eld::plugin::OutputSectionIteratorPlugin("ErrorOutAtCreatingSections") {}

  void Init(std::string options) override {}

  void processOutputSection(eld::plugin::OutputSection O) override {}

  Status Run(bool trace) override {
    if (getLinker()->isLinkStateCreatingSections())
      return Status::ERROR;
    return Status::SUCCESS;
  }

  std::string GetName() override { return "ErrorOutAtCreatingSections"; }

  std::string GetLastErrorAsString() override { return "Error!"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }
};

ELD_REGISTER_PLUGIN(ErrorOutAtCreatingSections)