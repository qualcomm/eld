#include "ControlFileSizePlugin.h"
#include "PluginVersion.h"
#include <cstring>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT CopyBlocksPlugin : public ControlFileSizePlugin {
public:
  CopyBlocksPlugin() : ControlFileSizePlugin("NULLBLOCKS"), Blocks() {}

  void Init(std::string Options) override {}

  void AddBlocks(Block B) override { Blocks.push_back(B); }

  Status Run(bool Verbose) override { return Plugin::Status::SUCCESS; }

  std::vector<Block> GetBlocks() override {
    Blocks.clear();
    return Blocks;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "NULLBLOCKS"; }

private:
  std::vector<Block> Blocks;
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["NULLBLOCKS"] = new CopyBlocksPlugin();
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
