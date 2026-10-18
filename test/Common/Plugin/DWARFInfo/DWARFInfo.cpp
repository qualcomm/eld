#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <cstdint>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT DWARFInfoPlugin : public OutputSectionIteratorPlugin {

public:
  DWARFInfoPlugin() : OutputSectionIteratorPlugin("GETOUTPUT") {}

  // Perform any initialization here
  void Init(std::string Options) override {
    for (InputFile &F : getLinker()->getInputFiles()) {
      if (!F.getSize())
        continue;

      // Exercise the object-aware DWARF reader used by plugins that inspect
      // input files directly. Parsing malformed optional metadata must not
      // abort the link.
      DWARFInfo DirectDI(F);
      inspectDIEs(DirectDI);

      eld::Expected<DWARFInfo> expDI =
          getLinker()->getDWARFInfoForInputFile(F, true);
      DWARFInfo DI = (expDI ? expDI.value() : DWARFInfo(nullptr));
      if (DI.hasDWARFContext()) {
        inspectDIEs(DI);
        std::cout << "good\n";
      } else {
        std::cout << "bad\n";
      }
    }
  }

  void processOutputSection(OutputSection O) override {}

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override { return Plugin::Status::SUCCESS; }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "GETOUTPUT"; }

private:
  // Exercise name decoding and type-size queries through either reader path.
  // Malformed or unsupported optional debug metadata must not abort the link.
  static void inspectDIEs(const DWARFInfo &DI) {
    for (DWARFUnit &Unit : DI.getDWARFUnits())
      for (DWARFDie &Die : Unit.getDIEs()) {
        (void)Die.getName();
        if (Die.isType()) {
          uint64_t Size;
          (void)Die.getSize(Size, 8);
        }
      }
  }
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new DWARFInfoPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
