#include "Defines.h"
#include "Diagnostics.h"
#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <iostream>

class DLL_A_EXPORT VisitInputHookPlugin : public eld::plugin::LinkerPlugin {
public:
  VisitInputHookPlugin() : eld::plugin::LinkerPlugin("VisitInputHookPlugin") {}

  void VisitInput(eld::plugin::InputFile IF) override {
    std::string path = IF.decoratedPath();
    if (path.find("bad.o") != std::string::npos) {
      getLinker()->reportDiag(
          eld::plugin::Diagnostic::error_file_does_not_exist(), path);
      return;
    }
    std::cout << "EVENT: VisitInput " << path
              << " numSections=" << IF.getSections().size() << "\n";
    std::cout << "VisitInput: " << path
              << " isArchiveFile=" << (IF.isArchiveFile() ? 1 : 0)
              << " isMember=" << (IF.isArchiveMember() ? 1 : 0) << "\n";
    std::cout << "VisitInput: " << path << " isNonBitcodeObject="
              << (IF.isObjectFile() && !IF.isBitcode() ? 1 : 0)
              << " isDynamic=" << (IF.isDynamicLibrary() ? 1 : 0)
              << " isBinary=" << (IF.isBinaryFile() ? 1 : 0)
              << " isBitcode=" << (IF.isBitcode() ? 1 : 0) << "\n";
  }

  void VisitSections(eld::plugin::InputFile IF) override {
    std::string path = IF.decoratedPath();
    std::cout << "EVENT: VisitSections " << path
              << " numSections=" << IF.getSections().size() << "\n";
  }
};

ELD_REGISTER_PLUGIN(VisitInputHookPlugin)
