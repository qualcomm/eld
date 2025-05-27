#include "Defines.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT RecomputeSizeChecker
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  RecomputeSizeChecker()
      : eld::plugin::OutputSectionIteratorPlugin("RecomputeSizeChecker") {}
  void Init(std::string Options) override {}
  void processOutputSection(eld::plugin::OutputSection O) override {}
  Status Run(bool Trace) override {
    if (!getLinker()->isLinkStateCreatingSections())
      return Status::SUCCESS;
    auto expFoo = getLinker()->getOutputSection("FOO");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expFoo);
    auto expBar = getLinker()->getOutputSection("BAR");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expBar);
    auto fooSect = expFoo.value();
    auto barSect = expBar.value();

    auto expInitialSizeFoo = fooSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expInitialSizeFoo);
    auto expInitialSizeBar = barSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expInitialSizeBar);

    auto initialFooSize = fooSect.getSize();
    auto initialBarSize = barSect.getSize();
    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";

    auto fooRules = fooSect.getLinkerScriptRules();
    auto barRules = barSect.getLinkerScriptRules();
    auto fooFirstRule = fooRules.front();
    auto barFirstRule = barRules.front();
    auto fooChunk = fooFirstRule.getChunks().front();
    auto expRemoveChunk = getLinker()->removeChunk(fooFirstRule, fooChunk);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRemoveChunk);
    auto expAddChunk = getLinker()->addChunk(barFirstRule, fooChunk);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expAddChunk);

    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";

    auto expRecomputeSizeFoo = fooSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRecomputeSizeFoo);
    auto expRecomputeSizeBar = barSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expRecomputeSizeBar);

    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";
    std::cout << "Sizes changed: " << (fooSect.getSize() < initialFooSize &&
                                       barSect.getSize() > initialBarSize)
              << "\n";
    return Status::SUCCESS;
  }
  void Destroy() override {}
  uint32_t GetLastError() override { return 0; }
  std::string GetLastErrorAsString() override { return "SUCCESS"; }
  std::string GetName() override { return "RecomputeSizeChecker"; }
};

ELD_REGISTER_PLUGIN(RecomputeSizeChecker)
