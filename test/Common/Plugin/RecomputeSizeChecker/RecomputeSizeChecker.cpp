#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT RecomputeSizeChecker : public eld::plugin::LinkerPlugin {
public:
  RecomputeSizeChecker() : eld::plugin::LinkerPlugin("RecomputeSizeChecker") {}
  void ActBeforePerformingLayout() override {
    auto expFoo = getLinker()->getOutputSection("FOO");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expFoo);
    auto expBar = getLinker()->getOutputSection("BAR");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expBar);
    auto fooSect = expFoo.value();
    auto barSect = expBar.value();

    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";

    auto fooRules = fooSect.getLinkerScriptRules();
    auto barRules = barSect.getLinkerScriptRules();
    auto fooFirstRule = fooRules.front();
    auto barFirstRule = barRules.front();
    auto fooChunk = fooFirstRule.getChunks().front();
    auto expRemoveChunk = getLinker()->removeChunk(fooFirstRule, fooChunk);
    auto expAddChunk = getLinker()->addChunk(barFirstRule, fooChunk);

    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";

    auto expRecomputeSizeFoo = fooSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expRecomputeSizeFoo);
    auto expRecomputeSizeBar = barSect.recomputeSize(*getLinker());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expRecomputeSizeBar);

    std::cout << "FOO size: " << fooSect.getSize() << "\n";
    std::cout << "BAR size: " << barSect.getSize() << "\n";
  }
};

ELD_REGISTER_PLUGIN(RecomputeSizeChecker)