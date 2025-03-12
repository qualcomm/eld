#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT SegmentInfo : public OutputSectionIteratorPlugin {

public:
  SegmentInfo() : OutputSectionIteratorPlugin("SegmentInfo") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::AfterLayout)
      return;
    if (O.getName() == ".dynamic")
      OutputSections.push_back(O);
  }

  Status Run(bool Trace) override {
    if (getLinker()->getState() != eld::plugin::LinkerWrapper::AfterLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    for (OutputSection &O : OutputSections) {
      auto ExpSegments = O.getSegments(*getLinker());
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), ExpSegments);
      for (Segment S : ExpSegments.value()) {
        std::cout << O.getName() << "\n";
        printSegmentInfo(S);
      }
    }
    auto Segments = getLinker()->getSegmentTable();
    ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), Segments);
    for (Segment &S : Segments.value()) {
      std::cout << S.getName() << "\n";
    }
    return eld::plugin::Plugin::Status::SUCCESS;
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "SegmentInfo"; }

private:
  void printSegmentInfo(Segment S) {
    std::cout << "\tname " << S.getName() << "\n";
    std::cout << "\tload segment " << S.isLoadSegment() << "\n";
    std::cout << "\tdynamic " << S.isDynamicSegment() << "\n";
    std::cout << "\toffset " << S.getOffset() << "\n";
    std::cout << "\tvaddr " << S.getVirtualAddress() << "\n";
    std::cout << "\tpaddr " << S.getPhysicalAddress() << "\n";
    std::cout << "\tfilesz " << S.getFileSize() << "\n";
    std::cout << "\tmemsz " << S.getMemorySize() << "\n";
    std::cout << "\tflag " << S.getSegmentFlags() << "\n";
    std::cout << "\talign " << S.getPageAlignment() << "\n";
    std::cout << "\tmax section align " << S.getMaxSectionAlign() << "\n";
    std::cout << "\t";
    auto Sections = S.getOutputSections(*getLinker());
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), Sections);
    for (OutputSection &O : Sections.value())
      std::cout << O.getName() << " ";
    std::cout << "\n";
  }

private:
  std::vector<OutputSection> OutputSections;
};

Plugin *ThisPlugin = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new SegmentInfo();
  return true;
}
PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  ThisPlugin = nullptr;
}
}
