#include "Defines.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "LinkerPlugin.h"
#include <unordered_map>

class DLL_A_EXPORT RMSectNameEmptyInputFile : public eld::plugin::LinkerPlugin {
public:
  RMSectNameEmptyInputFile()
      : eld::plugin::LinkerPlugin("RMSectNameEmptyInputFile") {}
  void Init(const std::string &option) override {
    std::unordered_map<uint64_t, std::string> RMSectNameEmptyInputFile;
    RMSectNameEmptyInputFile[3] = ".ruleMatchingName.foo";
    eld::plugin::InputFile IF(nullptr);
    auto E = getLinker()->setRuleMatchingSectionNameMap(
        IF, RMSectNameEmptyInputFile);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), E);
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
  ThisPlugin = new RMSectNameEmptyInputFile();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
