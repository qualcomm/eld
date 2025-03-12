#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <cstdint>
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT FindOutSectAddresses : public OutputSectionIteratorPlugin {
public:
  FindOutSectAddresses()
      : OutputSectionIteratorPlugin("FindOutSectAddresses") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override { return; }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::AfterLayout)
      return Status::SUCCESS;
    eld::Expected<std::vector<eld::plugin::OutputSection>> expOutSects =
        getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expOutSects);
    std::vector<eld::plugin::OutputSection> outSects = std::move(expOutSects.value());
    for (auto sect : outSects) {
      eld::Expected<uint64_t> expVirtualAddress =
          sect.getVirtualAddress(*getLinker());
      eld::Expected<uint64_t> expPhysicalAddress =
          sect.getPhysicalAddress(*getLinker());
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expVirtualAddress);
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expPhysicalAddress);
      std::cout << "Output section: " << sect.getName() << "\n";
      std::cout << "Virtual address: " << expVirtualAddress.value() << "\n";
      std::cout << "Physical address: " << expPhysicalAddress.value() << "\n";
      std::cout << "\n";
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "FindOutSectAddresses"; }
};

class DLL_A_EXPORT InvalidStateFindOutSectAddresses
    : public OutputSectionIteratorPlugin {
public:
  InvalidStateFindOutSectAddresses()
      : OutputSectionIteratorPlugin("InvalidStateFindOutSectAddresses") {}

  void Init(std::string Options) override {}

  void processOutputSection(OutputSection S) override {
    if (getLinker()->getState() != LinkerWrapper::BeforeLayout)
      return;
    if (S.getName() == "foo") {
      eld::Expected<uint64_t> expVirtualAddress =
          S.getVirtualAddress(*getLinker());
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
          getLinker(), expVirtualAddress);
      std::cout << "foo virtual address: " << expVirtualAddress.value() << "\n";
    }
  }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != LinkerWrapper::BeforeLayout)
      return Status::SUCCESS;
    eld::Expected<std::vector<eld::plugin::OutputSection>> expOutSects =
        getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expOutSects);
    std::vector<eld::plugin::OutputSection> outSects = std::move(expOutSects.value());
    for (auto sect : outSects) {
      eld::Expected<uint64_t> expVirtualAddress =
          sect.getVirtualAddress(*getLinker());
      eld::Expected<uint64_t> expPhysicalAddress =
          sect.getPhysicalAddress(*getLinker());
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expVirtualAddress);
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expPhysicalAddress);
      std::cout << "Output section: " << sect.getName() << "\n";
      std::cout << "Virtual address: " << expVirtualAddress.value() << "\n";
      std::cout << "Physical address: " << expPhysicalAddress.value() << "\n";
      std::cout << "\n";
    }
    return Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "InvalidStateFindOutSectAddresses"; }
};

std::unordered_map<std::string, Plugin *> Plugins;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  Plugins["FindOutSectAddresses"] = new FindOutSectAddresses();
  Plugins["InvalidStateFindOutSectAddresses"] =
      new InvalidStateFindOutSectAddresses();
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
