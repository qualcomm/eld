#include "Defines.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"

using namespace eld::plugin;

// Exercises OutputSection::setExcludeFromOverlapCheck: without it, two
// ALLOC sections placed at the same virtual address by a linker script
// are a hard "virtual address range overlaps" error. A plugin that excludes
// one of them from the overlap check during CreatingSegments suppresses
// that error for the excluded section.
class DLL_A_EXPORT SetExcludeFromOverlapCheck
    : public OutputSectionIteratorPlugin {
public:
  SetExcludeFromOverlapCheck()
      : OutputSectionIteratorPlugin("SetExcludeFromOverlapCheck") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {}

  Status Run(bool Trace) override {
    if (!getLinker()->isLinkStateCreatingSegments())
      return Status::SUCCESS;
    eld::Expected<OutputSection> ExpO = getLinker()->getOutputSection(".sec2");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpO);
    eld::plugin::OutputSection O = ExpO.value();
    eld::Expected<void> ExpSet = O.setExcludeFromOverlapCheck(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpSet);
    return Status::SUCCESS;
  }

  std::string GetName() override { return "SetExcludeFromOverlapCheck"; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }
};

ELD_REGISTER_PLUGIN(SetExcludeFromOverlapCheck)
