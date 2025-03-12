#include "LinkerPlugin.h"
#include "PluginVersion.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace eld::plugin;

class DLL_A_EXPORT InputFileLinkerPlugin : public LinkerPlugin {
public:
  InputFileLinkerPlugin() : LinkerPlugin("INPUTFILES") {}

  void Init(const std::string &Options) override;

  void ActBeforeSectionMerging() override;

  void Destroy() override {}

private:
  std::string getPath(InputFile I) const {
    std::string FileName = std::string(I.getFileName());
    if (I.isArchive())
      return FileName + "(" + I.getMemberName() + ")";
    return FileName;
  }

  std::string getFileType(const char *S, const char *T, std::size_t sz,
                          std::string Annotation) {
    int rc = std::memcmp(S, T, sz);
    if (rc == 0)
      return Annotation;
    return "Unknown";
  }

  const char *ELF = "\177ELF";
};

void InputFileLinkerPlugin::Init(const std::string &Options) {}


void InputFileLinkerPlugin::ActBeforeSectionMerging() {
  std::vector<InputFile> InputFiles = getLinker()->getInputFiles();
  for (auto &I : InputFiles) {
    if (!I.getSize())
      continue;
    const char *Data = I.getMemoryBuffer();
    std::cout << getPath(I) << "\t" << getFileType(Data, ELF, 4, "ELF") << "\t"
              << I.getSize() << "\n";
  }
  for (auto &I : InputFiles) {
    for (auto &S : I.getSections())
      std::cout << S.getName() << "\n";
  }
}

// Standard Linker plugin template.
std::unordered_map<std::string, PluginBase *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["INPUTFILES"] = new InputFileLinkerPlugin();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugins[std::string(T)];
}
void DLL_A_EXPORT Cleanup() {
  for (auto &A : Plugins)
    delete A.second;
  Plugins.clear();
}
}
