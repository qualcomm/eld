#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>
#include <unordered_map>

class AuxiliarySymbolNameTracePlugin : public eld::plugin::LinkerPlugin {
public:
  AuxiliarySymbolNameTracePlugin()
      : eld::plugin::LinkerPlugin("AuxiliarySymbolNameTracePlugin") {}

  void VisitSections(eld::plugin::InputFile IF) override {
    eld::plugin::AuxiliarySymbolNameMap nameMap;
    for (unsigned i = 0; i < 10; ++i) {
      nameMap[i] = "MyAuxSymName";
    }
    auto expSetAuxSymNameMap =
        getLinker()->setAuxiliarySymbolNameMap(IF, nameMap);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expSetAuxSymNameMap);
  }
};

ELD_REGISTER_PLUGIN(AuxiliarySymbolNameTracePlugin)
