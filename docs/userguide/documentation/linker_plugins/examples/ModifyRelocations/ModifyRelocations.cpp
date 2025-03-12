#include "ELD/PluginAPI/PluginVersion.h"
#include "ELD/PluginAPI/SectionIteratorPlugin.h"
#include <iostream>

class ModifyRelocations : public eld::plugin::SectionIteratorPlugin {
public:
  ModifyRelocations() : SectionIteratorPlugin("ModifyRelocations") {}

  // 'Init' callback hook can be used for initialization and preparations.
  // This plugin does not need any initialization or preparation.
  void Init(std::string cfg) override {}

  // 'processSection' callback hook of SectionIteratorPlugin is called for
  // each non-garbage collected section.
  void processSection(eld::plugin::Section S) override {}

  // 'Run' callback hook is called after all the 'processSection' callback hook
  // calls. It is called once for each section iterator plugin run.
  // This plugin does not need to run anything.
  eld::plugin::Plugin::Status Run(bool trace) override {
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  // 'Destroy' callback hook can be used for finalization and clean-up tasks.
  // It is called once for each section iterator plugin run.
  // This plugin does not need any finalization and clean-up.
  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "Success"; }

  std::string GetName() override { return "ModifyRelocations"; }

  eld::plugin::LinkerWrapper *getLinker() { return Linker; }
};

// LinkerPluginConfig allows to inspect and modify relocations.
class ModifyRelocationsPluginConfig : public eld::plugin::LinkerPluginConfig {
public:
  ModifyRelocationsPluginConfig(ModifyRelocations *P)
      : LinkerPluginConfig(P), P(P) {}

  void Init() override {
    // Register R_HEX_B22_PCREL relocation type.
    // Linker will call RelocCallBack callback hook function on each relocation
    // that is of a registered relocation type.
    std::string b22pcrel = "R_HEX_B22_PCREL";
    uint32_t relocationType =
        P->getLinker()->getRelocationHandler().getRelocationType(b22pcrel);
    P->getLinker()->registerReloc(relocationType);
  }

  // Relocation callback hook function.
  void RelocCallBack(eld::plugin::Use U) override {
    // Print relocation source section name and symbol names.
    std::string sourceSectionName = U.getSourceChunk().getName();
    std::cout << "Relocation callback. Source section: " << sourceSectionName
              << ", symbol: " << U.getName() << "\n";
    // Change relocation symbol from HelloWorld to HelloQualcomm.
    if (U.getSymbol().getName() == "HelloWorld") {
      eld::Expected<eld::plugin::Symbol> expHelloQualcommSymbol =
          P->getLinker()->getSymbol("HelloQualcomm");
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          P->getLinker(), expHelloQualcommSymbol);
      eld::plugin::Symbol helloQualcommSymbol =
          std::move(expHelloQualcommSymbol.value());
      U.resetSymbol(helloQualcommSymbol);
    }
  }

private:
  ModifyRelocations *P;
};

eld::plugin::Plugin *ThisPlugin = nullptr;
eld::plugin::LinkerPluginConfig *ThisPluginConfig = nullptr;

extern "C" {
// RegisterAll should initialize all the plugins and plugin configs that a
// plugin library aims to provide. Linker calls this function before running
// any plugins provided by the library.
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ModifyRelocations();
  ThisPluginConfig = new ModifyRelocationsPluginConfig(
      dynamic_cast<ModifyRelocations *>(ThisPlugin));
  return true;
}

// Linker calls this function to request an instance of the plugin
// with the plugin name pluginName. pluginName is provided in the plugin
// invocation command.
eld::plugin::Plugin *getPlugin(const char *pluginName) { return ThisPlugin; }

// Linker calls this function to request an instance of the plugin
// configuration for the plugin with the plugin name pluginName.
// pluginName is provided in the plugin invocation command.
eld::plugin::LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *pluginName) {
  return ThisPluginConfig;
}

// Cleanup should free all the resources owned by a plugin library.
// Linker calls this function after all runs of the plugins provided
// by the library have completed.
void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  if (ThisPluginConfig)
    delete ThisPluginConfig;
}
}