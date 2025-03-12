#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include <iostream>
#include <unordered_map>

#define show(x)                                                                \
  ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(       \
      getLinker(), x);                                                         \
  std::cout << #x << ": " << x.value() << "\n";

class DoesRuleMatchWithSectionPlugin : public eld::plugin::LinkerPlugin {
public:
  DoesRuleMatchWithSectionPlugin()
      : eld::plugin::LinkerPlugin("DoesRuleMatchWithSectionPlugin") {}

  void VisitSections(eld::plugin::InputFile IF) override {
    if (endswith(IF.getFileName(), "1.o")) {
      std::unordered_map<uint64_t, std::string> ruleMatchingSectNameMap;
      uint64_t barIndex = 0;
      for (auto S : IF.getSections()) {
        if (S.getName() == ".text.bar") {
          BarSect = S;
          barIndex = S.getIndex();
          break;
        }
      }
      ruleMatchingSectNameMap[barIndex] = ".ruleMatchingName.foo";
      auto E = getLinker()->setRuleMatchingSectionNameMap(
          IF, ruleMatchingSectNameMap);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), E);
    }
  }

  void ActBeforeSectionMerging() override {
    eld::plugin::LinkerScriptRule R = BarSect.getLinkerScriptRule();
    show(getLinker()->doesRuleMatchWithSection(R, BarSect));
    show(getLinker()->doesRuleMatchWithSection(R, BarSect,
                                               /*doNotUseRMName=*/true));
  }

private:
  bool endswith(const std::string &s, const std::string &suffix) {
    if (s.size() < suffix.size())
      return false;
    return s.substr(s.size() - suffix.size()) == suffix;
  }
  eld::plugin::Section BarSect;
};

eld::plugin::PluginBase *ThisPlugin = nullptr;

extern "C" {
bool RegisterAll() {
  ThisPlugin = new DoesRuleMatchWithSectionPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
