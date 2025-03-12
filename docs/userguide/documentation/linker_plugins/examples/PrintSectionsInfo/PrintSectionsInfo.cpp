#include "ELD/PluginAPI/PluginVersion.h"
#include "ELD/PluginAPI/SectionMatcherPlugin.h"
#include <iostream>
#include <set>
#include <string>

class PrintSectionsInfo : public eld::plugin::SectionMatcherPlugin {
public:
  PrintSectionsInfo() : eld::plugin::SectionMatcherPlugin("PrintSectionsInfo") {}

  // 'Init' callback hook can be used for initialization and preparations.
  // We will read the configuration file here.
  void Init(std::string cfg) override {
    // 'Linker' is an inherited LinkerWrapper member variable. LinkerWrapper
    // is to be used to make any calls to the Linker.
    // LinkerWrapper::findConfigFile searches the file in standard paths and
    // returns a resolved path if the file is found.
    std::string configPath = Linker->findConfigFile(cfg);
    auto expINIFile = Linker->readINIFile(configPath);
    // If plugin configuration file cannot be read, then report the error,
    // set linker to fatal error and return.
    if (!expINIFile) {
      Linker->reportDiagEntry(std::move(expINIFile.error()));
      Linker->setLinkerFatalError();
      return;
    }
    eld::plugin::INIFile I = std::move(expINIFile.value());

    // Read patterns from the plugin configuration file and store
    // them in a member variable. These patterns will be used later
    // to decide which sections information should be printed.
    auto sections = I.getSection("sections");
    for (auto item : sections) {
      if (item.second == "1")
        m_SectionPatterns.insert(item.first);
    }
  }

  // 'processSection' callback hook is called for each input section.
  void processSection(eld::plugin::Section S) override {
    if (shouldPrintSectionInfo(S)) {
      std::cout << S.getName() << "\n";
      std::cout << "Input file: " << S.getInputFile().getFileName() << "\n";
      std::cout << "Section index: " << S.getIndex() << "\n";
      std::cout << "Section alignment: " << S.getAlignment() << "\n";
      std::cout << "\n";
    }
  }

  // 'Run' callback hook is called after 'processSection' callback hook calls.
  // It is called once for each section iterator plugin run.
  eld::plugin::Plugin::Status Run(bool trace) override {
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "PrintSectionsInfo"; }

private:
  // Returns true if the section information should be printed.
  bool shouldPrintSectionInfo(eld::plugin::Section S) {
    // If the section name matches the pattern specified in the plugin
    // configuration file then return true.
    for (auto pattern : m_SectionPatterns) {
      if (S.matchPattern(pattern))
        return true;
    }
    return false;
  }

  // Stores section name patterns.
  std::set<std::string> m_SectionPatterns;
};

eld::plugin::Plugin *ThisPlugin = nullptr;

extern "C" {
// RegisterAll should initialize all the plugins that the plugin library aims
// to provide. Linker calls this function before running any plugins provided
// by the library.
bool RegisterAll() {
  ThisPlugin = new PrintSectionsInfo{};
  return true;
}

// Linker calls this function to request an instance of the plugin
// with the plugin name pluginName. pluginName is provided in the plugin
// invocation command.
eld::plugin::Plugin *getPlugin(const char *pluginName) { return ThisPlugin; }

// Cleanup should free all the resources owned by the plugin library.
// Linker calls this function after all runs of the plugins provided
// by the library have completed.
void Cleanup() {
  if (!ThisPlugin)
    return;
  delete ThisPlugin;
  ThisPlugin = nullptr;
}
}