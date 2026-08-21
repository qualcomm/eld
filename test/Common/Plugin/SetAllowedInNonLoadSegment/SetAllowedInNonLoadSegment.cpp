#include "Defines.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"

using namespace eld::plugin;

// Exercises OutputSection::setAllowedInNonLoadSegment: allows ".overlay",
// an ALLOC, non-zero-size section placed (via a linker script) in a
// PT_NULL segment, to avoid the "loadable section not in load segment"
// error the linker would otherwise raise for it.
class DLL_A_EXPORT SetAllowedInNonLoadSegment
    : public OutputSectionIteratorPlugin {
public:
  SetAllowedInNonLoadSegment()
      : OutputSectionIteratorPlugin("SetAllowedInNonLoadSegment") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {}

  Status Run(bool Trace) override {
    if (!getLinker()->isLinkStateCreatingSegments())
      return Status::SUCCESS;
    eld::Expected<OutputSection> ExpO =
        getLinker()->getOutputSection(".overlay");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpO);
    eld::plugin::OutputSection O = ExpO.value();
    eld::Expected<void> ExpSet = O.setAllowedInNonLoadSegment(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpSet);
    return Status::SUCCESS;
  }

  std::string GetName() override { return "SetAllowedInNonLoadSegment"; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }
};

ELD_REGISTER_PLUGIN(SetAllowedInNonLoadSegment)
