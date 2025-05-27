#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT AddedSectionOverrides
    : public OutputSectionIteratorPlugin {
public:
  AddedSectionOverrides()
      : OutputSectionIteratorPlugin("AddedSectionOverrides") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::CreatingSections)
      return Status::SUCCESS;
    eld::Expected<eld::plugin::OutputSection> expO =
        getLinker()->getOutputSection(".data");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expO);
    eld::plugin::OutputSection O = expO.value();
    for (auto &R : O.getLinkerScriptRules()) {
      for (auto &S : R.getSections()) {
        eld::Expected<void> expSetOutSect =
            getLinker()->setOutputSection(S, ".bar");
        ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expSetOutSect);
      }
    }
    getLinker()->finishAssignOutputSections();
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "AddedSectionOverrides"; }
};


ELD_REGISTER_PLUGIN(AddedSectionOverrides)
