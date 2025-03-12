#include "Defines.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>
#include <unordered_map>

class DLL_A_EXPORT RuleMatchingSectNameMap : public eld::plugin::LinkerPlugin {
public:
  RuleMatchingSectNameMap()
      : eld::plugin::LinkerPlugin("RuleMatchingSectNameMap") {}

  void VisitSections(eld::plugin::InputFile IF) override {
    if (endswith(IF.getFileName(), "1.o")) {
      std::unordered_map<uint64_t, std::string> ruleMatchingSectNameMap;
      uint64_t barIndex = 0;
      for (auto S : IF.getSections()) {
        if (S.getName() == ".text.bar")
          barIndex = S.getIndex();
      }
      ruleMatchingSectNameMap[barIndex] = ".ruleMatchingName.foo";
      auto E = getLinker()->setRuleMatchingSectionNameMap(
          IF, ruleMatchingSectNameMap);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), E);
    }
  }

private:
  bool endswith(const std::string &s, const std::string &suffix) {
    if (s.size() < suffix.size())
      return false;
    return s.substr(s.size() - suffix.size()) == suffix;
  }
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new RuleMatchingSectNameMap();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
