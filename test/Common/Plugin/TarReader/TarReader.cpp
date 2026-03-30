#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include "TarWriter.h"
#include <iostream>
#include <string>

using namespace eld::plugin;

namespace {

static bool endsWith(const std::string &S, const std::string &Suffix) {
  if (Suffix.size() > S.size())
    return false;
  return S.compare(S.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
}

class DLL_A_EXPORT TarReaderPlugin : public LinkerPlugin {
public:
  TarReaderPlugin() : LinkerPlugin("TarReader") {}

  void Init(const std::string &Options) override {
    (void)Options;
    std::string TarFile = "tar-reader-api.tar";
    auto TW = getLinker()->getTarWriter(TarFile);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), TW);

    std::string RegularEntryContents = "Regular entry contents";
    auto RegularEntryBuffer = MemoryBuffer::getBuffer(
        "regular_entry.txt",
        reinterpret_cast<const uint8_t *>(RegularEntryContents.data()),
        RegularEntryContents.size(), true);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), RegularEntryBuffer);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), TW->addBufferToTar(RegularEntryBuffer.value()));

    std::string NestedEntryContents = "Nested entry contents";
    auto NestedEntryBuffer = MemoryBuffer::getBuffer(
        "nested/nested_entry.json",
        reinterpret_cast<const uint8_t *>(NestedEntryContents.data()),
        NestedEntryContents.size(), true);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), NestedEntryBuffer);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), TW->addBufferToTar(NestedEntryBuffer.value()));

    auto Tar = getLinker()->openTarFile(TarFile);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Tar);

    auto Names = Tar->listEntries();
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Names);

    bool HasRegularEntry = false;
    bool HasNestedEntry = false;
    size_t ReadableEntries = 0;
    for (const std::string &Name : Names.value()) {
      HasRegularEntry = HasRegularEntry ||
                        endsWith(Name, "/regular_entry.txt") ||
                        Name == "regular_entry.txt";
      HasNestedEntry = HasNestedEntry ||
                       endsWith(Name, "/nested/nested_entry.json") ||
                       Name == "nested/nested_entry.json";
      auto Content = Tar->readEntryContents(Name);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), Content);
      ++ReadableEntries;
    }

    auto RegularEntry = Tar->readEntryContents("regular_entry.txt");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), RegularEntry);

    auto NestedEntry = Tar->readEntryContents("nested/nested_entry.json");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), NestedEntry);

    std::cout << "tar-reader-entry-count=" << Names.value().size() << "\n";
    std::cout << "tar-reader-readable-entry-count=" << ReadableEntries << "\n";
    std::cout << "tar-reader-has-regular-entry="
              << (HasRegularEntry ? "yes" : "no") << "\n";
    std::cout << "tar-reader-has-nested-entry="
              << (HasNestedEntry ? "yes" : "no") << "\n";
    std::cout << "tar-reader-regular-entry="
              << std::string(RegularEntry.value()) << "\n";
    std::cout << "tar-reader-nested-entry=" << std::string(NestedEntry.value())
              << "\n";
  }
};

} // namespace

ELD_REGISTER_PLUGIN(TarReaderPlugin)
