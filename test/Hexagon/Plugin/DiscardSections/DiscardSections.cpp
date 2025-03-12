#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include "SectionMatcherPlugin.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

// All plugins must be derived from one of the linker defined plugins.
// In this case it is the SectionIteratorPlugin defined in
// "SectionIteratorPlugin.h"
class DLL_A_EXPORT DiscardPlugin : public SectionIteratorPlugin {

public:
  // This constructor takes no arguments;
  // the base class will take the name we will call this plugin
  DiscardPlugin() : SectionIteratorPlugin("DISCARD") {}

  // This init doesn't do anything, but we could add code here
  void Init(std::string Options) override {}

  // This function will be called whenever the linker processes a section.
  // We will add this section to our vector of sections for later use.
  void processSection(eld::plugin::Section S) override {
    std::string Name = S.getName();
    if (S.matchPattern(".text.f1*"))
      m_Sections.push_back(S);
    if (S.matchPattern(".rela.text.f1*"))
      m_Sections.push_back(S);
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    for (auto &S : m_Sections) {
      std::cout << "Discarding section " << S.getName() << "\n";
      S.markAsDiscarded();
    }
    // We could track a private variable for return status, but in this example
    // we always return success
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "DISCARD"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

class DLL_A_EXPORT DiscardPluginSM : public SectionMatcherPlugin {

public:
  // This constructor takes no arguments;
  // the base class will take the name we will call this plugin
  DiscardPluginSM() : SectionMatcherPlugin("DiscardPluginSM") {}

  // This init doesn't do anything, but we could add code here
  void Init(std::string Options) override {}

  // This function will be called whenever the linker processes a section.
  // We will add this section to our vector of sections for later use.
  void processSection(eld::plugin::Section S) override {
    std::string Name = S.getName();
    if (S.matchPattern(".text.f1*"))
      m_Sections.push_back(S);
    if (S.matchPattern(".rela.text.f1*"))
      m_Sections.push_back(S);
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    for (auto &S : m_Sections) {
      std::cout << "Discarding section " << S.getName() << "\n";
      S.markAsDiscarded();
    }
    // We could track a private variable for return status, but in this example
    // we always return success
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "DiscardPluginSM"; }

private:
  std::vector<eld::plugin::Section> m_Sections;
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["DISCARD"] = new DiscardPlugin{};
  Plugins["DiscardPluginSM"] = new DiscardPluginSM{};
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return Plugins[T]; }
void DLL_A_EXPORT Cleanup() {
  for (auto &item : Plugins) {
    if (item.second)
      delete item.second;
  }
  Plugins.clear();
}
}
