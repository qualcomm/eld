#include "Defines.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"

using namespace eld::plugin;

// Exercises LinkerWrapper::setSymbolAddress. In the "valid" mode (the
// default), it is called in the AfterLayout state, where it is valid, to
// detach "foo" from its fragment and make it an absolute symbol with a
// fixed value. In the "invalid" mode, it is instead called during
// CreatingSections, to exercise the invalid-link-state error path.
class DLL_A_EXPORT SetSymbolAddress : public OutputSectionIteratorPlugin {
public:
  SetSymbolAddress() : OutputSectionIteratorPlugin("SetSymbolAddress") {}

  void Init(std::string Options) override {
    CallInCreatingSections = (Options == "invalid");
  }

  void processOutputSection(OutputSection O) override {}

  Status Run(bool Trace) override {
    if (CallInCreatingSections && getLinker()->isLinkStateCreatingSections())
      return callSetSymbolAddress(0x1234);
    if (!CallInCreatingSections && getLinker()->isLinkStateAfterLayout())
      return callSetSymbolAddress(0x12345678);
    return Status::SUCCESS;
  }

  Status callSetSymbolAddress(uint64_t Addr) {
    eld::Expected<Symbol> ExpS = getLinker()->getSymbol("foo");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpS);
    eld::Expected<void> ExpSet =
        getLinker()->setSymbolAddress(ExpS.value(), Addr);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpSet);
    return Status::SUCCESS;
  }

  std::string GetName() override { return "SetSymbolAddress"; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

private:
  bool CallInCreatingSections = false;
};

ELD_REGISTER_PLUGIN(SetSymbolAddress)
