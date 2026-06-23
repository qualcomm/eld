#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <cstdint>
#include <cstring>
#include <iostream>

class DLL_A_EXPORT ReplaceSymbolContentPlugin
    : public eld::plugin::LinkerPlugin {
public:
  ReplaceSymbolContentPlugin()
      : eld::plugin::LinkerPlugin("ReplaceSymbolContentPlugin") {}

  void Init(const std::string &Options) override {}

  void ActBeforeWritingOutput() override {
    replaceIfLive("foo_data");
    replaceIfLive("dead_data");
  }

private:
  void replaceIfLive(const std::string &SymName) {
    eld::Expected<eld::plugin::Symbol> ExpSym = getLinker()->getSymbol(SymName);
    if (!ExpSym) {
      getLinker()->reportDiagEntry(std::move(ExpSym.error()));
      return;
    }
    eld::plugin::Symbol Sym = ExpSym.value();

    if (Sym.isGarbageCollected() || Sym.isUndef()) {
      std::cout << "ReplaceSymbolContentPlugin: skipped " << SymName
                << " (garbage collected)\n";
      return;
    }

    uint32_t size = Sym.getSize();
    uint8_t *Buf =
        reinterpret_cast<uint8_t *>(getLinker()->getUninitBuffer(size));
    std::memset(Buf, 0xAB, size);
    eld::Expected<void> ExpReplace =
        getLinker()->replaceSymbolContent(Sym, Buf, size);
    if (!ExpReplace) {
      getLinker()->reportDiagEntry(std::move(ExpReplace.error()));
      return;
    }
    std::cout << "ReplaceSymbolContentPlugin: replaced " << SymName << " ("
              << size << " bytes)\n";
  }

  void Destroy() override {}
};

ELD_REGISTER_PLUGIN(ReplaceSymbolContentPlugin)
