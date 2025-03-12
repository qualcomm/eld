#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include "SectionIteratorPlugin.h"
#include <iostream>
#include <string>

using namespace eld::plugin;

class DLL_A_EXPORT ExcludeSymbol : public SectionIteratorPlugin {

  std::vector<Section> Sections;

public:
  ExcludeSymbol() : SectionIteratorPlugin("EXCLUDESYMBOL") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processSection(Section O) override { Sections.push_back(O); }

  void removeLocals() {
    for (Section &S : Sections)
      for (Symbol &Sym : S.getSymbols())
        if (Sym.isLocal())
          getLinker()->removeSymbolTableEntry(Sym);
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    eld::Expected<eld::plugin::Symbol> expS = getLinker()->getSymbol("foo");
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expS);
    eld::plugin::Symbol S = std::move(expS.value());

    getLinker()->removeSymbolTableEntry(S);
    removeLocals();
    return SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "EXCLUDESYMBOL"; }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ExcludeSymbol();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
