#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginVersion.h"
#include <iostream>

using namespace eld::plugin;

class DLL_A_EXPORT MemoryRegionAPI : public LinkerPlugin {
public:
  MemoryRegionAPI() : LinkerPlugin("MemoryRegionAPI") {}

  void ActBeforeWritingOutput() override {
    const MemoryRegion Empty;
    std::cout << "empty: " << bool(Empty) << " " << Empty.getName().size()
              << " " << Empty.getAttributes().size() << " "
              << Empty.getOrigin().value() << " " << Empty.getLength().value()
              << "\n";
    const OutputSection EmptySection(nullptr);
    std::cout << "empty section: " << bool(EmptySection.getVMAMemoryRegion())
              << " " << bool(EmptySection.getLMAMemoryRegion()) << "\n";
    const auto Regions = getLinker()->getMemoryRegions();
    std::cout << "regions: " << Regions.size() << "\n";
    for (const auto &Region : Regions) {
      auto Origin = Region.getOrigin();
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Origin);
      auto Length = Region.getLength();
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Length);
      std::cout << "region: " << Region.getName() << " " << Origin.value()
                << " " << Length.value() << " [" << Region.getAttributes()
                << "]\n";
    }
    auto Sections = getLinker()->getAllOutputSections();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Sections);
    for (const auto &Section : Sections.value()) {
      const auto VMA = Section.getVMAMemoryRegion();
      const auto LMA = Section.getLMAMemoryRegion();
      if (!VMA && !LMA)
        continue;
      std::cout << "section: " << Section.getName() << " VMA=[" << VMA.getName()
                << "] LMA=[" << LMA.getName() << "]\n";
      for (const auto &Region : Regions) {
        if (VMA && VMA.getName() == Region.getName() &&
            VMA.getMemoryRegion() != Region.getMemoryRegion())
          std::cout << "invalid VMA handle\n";
        if (LMA && LMA.getName() == Region.getName() &&
            LMA.getMemoryRegion() != Region.getMemoryRegion())
          std::cout << "invalid LMA handle\n";
      }
    }
  }
};

ELD_REGISTER_PLUGIN(MemoryRegionAPI)
