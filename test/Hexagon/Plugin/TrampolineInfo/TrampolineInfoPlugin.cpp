#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>

class TrampolineInfoPlugin : public eld::plugin::LinkerPlugin {
public:
  TrampolineInfoPlugin() : eld::plugin::LinkerPlugin("TrampolineInfoPlugin") {}
  void ActBeforeWritingOutput() override {
    auto expAllOutSects = getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expAllOutSects);
    for (auto outSect : expAllOutSects.value()) {
      auto trampolines = outSect.getStubs();
      for (auto T : trampolines) {
        std::cout << "Output section '" << outSect.getName()
                  << "' has trampoline '" << T.getStubSymbol().getName()
                  << "' for target symbol '" << T.getTargetSymbol().getName()
                  << "'\n";
      }
    }
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new TrampolineInfoPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
