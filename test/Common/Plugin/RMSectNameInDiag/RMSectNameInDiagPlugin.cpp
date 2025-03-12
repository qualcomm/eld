#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <iostream>
#include <unordered_map>

class RMSectNameInDiagPlugin : public eld::plugin::LinkerPlugin {
public:
  RMSectNameInDiagPlugin() : eld::plugin::LinkerPlugin("RMSectNameInDiagPlugin") {}

  void Init(const std::string &options) override {
    getLinker()->showRuleMatchingSectionNameInDiagnostics();
  }

  void VisitSections(eld::plugin::InputFile IF) override {
    if (endswith(IF.getFileName(), "1.o")) {
      std::unordered_map<uint64_t, std::string> RMSectNameInDiagPlugin;
      uint64_t barIndex = 0;
      for (auto S : IF.getSections()) {
        if (S.getName() == ".text.bar")
          barIndex = S.getIndex();
      }
      RMSectNameInDiagPlugin[barIndex] = ".text.myfoo";
      auto E = getLinker()->setRuleMatchingSectionNameMap(
          IF, RMSectNameInDiagPlugin);
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
bool RegisterAll() {
  ThisPlugin = new RMSectNameInDiagPlugin();
  return true;
}

eld::plugin::PluginBase *getPlugin(const char *T) { return ThisPlugin; }

void Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
