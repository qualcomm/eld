#include "LinkerPlugin.h"
#include "eld/Support/StringUtils.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include <charconv>
#include <string>

namespace {

class LinkerPluginVersion : public eld::plugin::LinkerPlugin {
public:
  LinkerPluginVersion() : LinkerPlugin("LinkerPluginVersion") {}
};

std::unique_ptr<eld::plugin::PluginBase> Plugin;

} // namespace

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  if (!Plugin)
    Plugin = std::make_unique<LinkerPluginVersion>();
  return true;
}

void DLL_A_EXPORT Cleanup() { Plugin = nullptr; }

eld::plugin::PluginBase DLL_A_EXPORT *getPlugin(const char *T) {
  return Plugin && Plugin->GetName() == T ? Plugin.get() : nullptr;
}

eld::plugin::LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *) {
  return nullptr;
}

#ifndef NO_VERSION

namespace {

unsigned int parseNumber(const std::string &s) {
  unsigned int value;
  auto result = std::from_chars(s.data(), s.data() + s.size(), value);
  if (result.ec != std::errc() || result.ptr != s.data() + s.size())
    return 0;
  return value;
}

} // namespace

void DLL_A_EXPORT getPluginAPIVersion(unsigned int *Major, unsigned int *Minor) {
  const char *VersionMajorString = getenv("ELD_TEST_LINKER_PLUGIN_VERSION_MAJOR");
  const char *VersionMinorString = getenv("ELD_TEST_LINKER_PLUGIN_VERSION_MINOR");
  *Major = VersionMajorString ? parseNumber(VersionMajorString) : 0;
  *Minor = VersionMinorString ? parseNumber(VersionMinorString) : 0;
}

#endif
}

