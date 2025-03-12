#include "Defines.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <unordered_map>

namespace {
bool endswith(const std::string &s, const std::string &suffix) {
  if (s.size() < suffix.size())
    return false;
  return s.substr(s.size() - suffix.size()) == suffix;
}
} // namespace

class DLL_A_EXPORT RMSectNameMapA : public eld::plugin::LinkerPlugin {
public:
  RMSectNameMapA() : eld::plugin::LinkerPlugin("RMSectNameMapA") {}
  void VisitSections(eld::plugin::InputFile IF) override {
    if (!endswith(IF.getFileName(), "1.o"))
      return;
    auto inputFiles = getLinker()->getInputFiles();
    std::unordered_map<uint64_t, std::string> RMSectNameMap;
    RMSectNameMap[3] = ".ruleMatchingName.foo";

    auto E = getLinker()->setRuleMatchingSectionNameMap(IF, RMSectNameMap);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), E);
  }
};

class DLL_A_EXPORT RMSectNameMapB : public eld::plugin::LinkerPlugin {
public:
  RMSectNameMapB() : eld::plugin::LinkerPlugin("RMSectNameMapB") {}
  void VisitSections(eld::plugin::InputFile IF) override {
    if (!endswith(IF.getFileName(), "1.o"))
      return;
    auto inputFiles = getLinker()->getInputFiles();
    std::unordered_map<uint64_t, std::string> RMSectNameMap;
    RMSectNameMap[3] = ".ruleMatchingName.foo";
    auto E = getLinker()->setRuleMatchingSectionNameMap(IF, RMSectNameMap);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), E);
  }
};

std::unordered_map<std::string, eld::plugin::PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["RMSectNameMapA"] = new RMSectNameMapA();
  Plugins["RMSectNameMapB"] = new RMSectNameMapB();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }

void DLL_A_EXPORT Cleanup() {
  for (auto elem : Plugins) {
    delete elem.second;
    elem.second = nullptr;
  }
}
}
