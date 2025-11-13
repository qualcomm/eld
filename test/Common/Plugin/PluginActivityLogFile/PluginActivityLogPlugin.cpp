// A LinkerPlugin that exercises various PluginOp operations so that
// PluginActivityLog captures them. This is a minimal smoke plugin.

// Exercise multiple PluginOp recording paths using a LinkerPlugin.

#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginADT.h"
#include "PluginBase.h"
#include "PluginVersion.h"
#include <unordered_map>

class DLL_A_EXPORT OutSectIterPluginActLog
    : public eld::plugin::OutputSectionIteratorPlugin {
public:
  eld::plugin::Section bazSection;

  OutSectIterPluginActLog()
      : OutputSectionIteratorPlugin("OutSectIterPluginActLog") {}

  std::string GetName() override { return "OutSectIterPluginActLog"; }

  void Init(std::string) override {
    if (!getLinker()->isLinkStateBeforeLayout())
      return;
    auto inputFiles = getLinker()->getInputFiles();
    for (auto IF : inputFiles) {
      for (auto S : IF.getSections()) {
        if (S.getName().find(".baz") != std::string::npos) {
          bazSection = S;
          break;
        }
      }
      if (bazSection) {
        auto expSetOutSect = getLinker()->setOutputSection(bazSection, ".bar");
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expSetOutSect);
        auto expFinishAssignOutSect = getLinker()->finishAssignOutputSections();
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(),
                                               expFinishAssignOutSect);
      }
    }
  }

  void processOutputSection(eld::plugin::OutputSection O) override { return; }

  // Before section merging: perform section overrides and try removing a chunk.
  Status Run(bool verbose) override {
    if (!getLinker()->isLinkStateCreatingSections())
      return Status::SUCCESS;
    auto fooOutSect = getLinker()->getOutputSection(".foo");
    auto barOutSect = getLinker()->getOutputSection(".bar");
    auto fooRules = fooOutSect->getLinkerScriptRules();
    auto barRules = barOutSect->getLinkerScriptRules();
    auto fooChunks = fooRules.front().getChunks();
    auto barChunks = barRules.front().getChunks();
    barChunks.insert(barChunks.end(), fooChunks.begin(), fooChunks.end());

    auto expFooRulesUpdate = getLinker()->updateChunks(fooRules.front(), {});
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expFooRulesUpdate);

    auto expBarRulesUpdate =
        getLinker()->updateChunks(barRules.front(), barChunks);
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expBarRulesUpdate);

    auto expBazSymbol = getLinker()->getSymbol("baz");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expBazSymbol);
    getLinker()->removeSymbolTableEntry(expBazSymbol.value());
    getLinker()->addLinkStat("PluginName", "OutSectIterPluginActLog");

    return Status::SUCCESS;
  }

  uint32_t GetLastError() override { return Status::SUCCESS; }

  std::string GetLastErrorAsString() override { return "Success"; }

  void Destroy() override {}
};

class LPPluginActLog : public eld::plugin::LinkerPlugin {
public:
  LPPluginActLog() : eld::plugin::LinkerPlugin("LPPluginActLog") {}

  void Init(const std::string &) override {
    auto expAddOption = getLinker()->registerCommandLineOption(
        "--custom-plugin-option", /*HasValue=*/true,
        [](const std::string &, const std::optional<std::string> &) {});
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), expAddOption);
  }
};

std::unordered_map<std::string, eld::plugin::PluginBase *> plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  plugins["OutSectIterPluginActLog"] = new OutSectIterPluginActLog();
  plugins["LPPluginActLog"] = new LPPluginActLog();
  return true;
}

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *name) {
  return plugins[name];
}

void DLL_A_EXPORT Cleanup() {
  for (auto &p : plugins) {
    delete p.second;
    p.second = nullptr;
  }
  plugins.clear();
}
}
