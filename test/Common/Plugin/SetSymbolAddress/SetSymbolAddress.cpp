#include "Defines.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"

using namespace eld::plugin;

// Exercises LinkerWrapper::setSymbolAddress. In the "valid" mode (the
// default), it is called in the AfterLayout state, where it is valid, to
// detach "data" from its fragment and make it an absolute symbol with a
// fixed value. In the "invalid" mode, it is instead called during
// CreatingSections, to exercise the invalid-link-state error path. In the
// "notfound" mode, it targets "bar", a symbol that is exported by a shared
// object but never referenced by any relocatable object file, so its
// ResolveInfo is findable but has no output symbol -- exercising the
// "failed to set address" error path. In the "function" mode, it targets
// "foo", a function symbol, to exercise the function-symbol rejection error
// path. In the "ifunc" mode, it targets "ifunc", an STT_GNU_IFUNC symbol, to
// exercise that ifunc symbols are rejected the same way as regular function
// symbols. In the "outofrange" mode, it targets "data" with an address
// beyond the maximum address supported by 32-bit targets, to exercise the
// address-range rejection error path.
class DLL_A_EXPORT SetSymbolAddress : public OutputSectionIteratorPlugin {
public:
  SetSymbolAddress() : OutputSectionIteratorPlugin("SetSymbolAddress") {}

  void Init(std::string Options) override {
    CallInCreatingSections = (Options == "invalid");
    if (Options == "notfound")
      TargetSymbol = "bar";
    if (Options == "function")
      TargetSymbol = "foo";
    if (Options == "ifunc")
      TargetSymbol = "ifunc";
    if (Options == "outofrange")
      OutOfRange = true;
  }

  void processOutputSection(OutputSection O) override {}

  Status Run(bool Trace) override {
    if (CallInCreatingSections && getLinker()->isLinkStateCreatingSections())
      return callSetSymbolAddress(0x1234);
    if (!CallInCreatingSections && getLinker()->isLinkStateAfterLayout())
      return callSetSymbolAddress(OutOfRange ? 0x100000000ULL : 0x12345678);
    return Status::SUCCESS;
  }

  Status callSetSymbolAddress(uint64_t Addr) {
    eld::Expected<Symbol> ExpS = getLinker()->getSymbol(TargetSymbol);
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
  bool OutOfRange = false;
  std::string TargetSymbol = "data";
};

ELD_REGISTER_PLUGIN(SetSymbolAddress)
