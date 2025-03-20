#include "Expected.h"
#include "LinkerWrapper.h"
#include "OutputSectionIteratorPlugin.h"
#include "PluginVersion.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace eld::plugin;

class DLL_A_EXPORT ChangeSymbolPlugin : public OutputSectionIteratorPlugin {

public:
  ChangeSymbolPlugin() : OutputSectionIteratorPlugin("CHANGESYMBOL") {}

  // Perform any initialization here
  void Init(std::string Options) override {}

  void processOutputSection(OutputSection O) override {
    if (getLinker()->getState() == LinkerWrapper::BeforeLayout)
      return;

    if (getLinker()->getState() == LinkerWrapper::CreatingSections)
      return;

    if (getLinker()->getState() == LinkerWrapper::AfterLayout)
      return;
  }

  // After the linker lays out the image, but before it creates the elf file,
  // it will call this run function.
  Status Run(bool Trace) override {
    if (getLinker()->getState() == LinkerWrapper::BeforeLayout)
      return eld::plugin::Plugin::Status::SUCCESS;
    if (getLinker()->getState() == LinkerWrapper::AfterLayout) {
      eld::Expected<uint32_t> expChecksum =
          getLinker()->getImageLayoutChecksum();
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expChecksum);
      uint32_t checksum = expChecksum.value();
      std::cerr << "Checksum for Image Layout = " << checksum << "\n";
      eld::Expected<eld::plugin::Symbol> expS =
          getLinker()->getSymbol("my_data_symbol");
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expS);
      eld::plugin::Symbol S = std::move(expS.value());

      std::cerr << "Data symbol in the Linker has address = " << S.getAddress()
                << "\n";
      eld::Expected<eld::plugin::Symbol> expS0 =
          getLinker()->getSymbol("my_data_symbol_inside");
      ELDEXP_REPORT_AND_RETURN_ERROR_IF_ERROR(getLinker(), expS0);
      eld::plugin::Symbol S0 = std::move(expS0.value());

      std::cerr << "Data symbol in the Linker has address = " << S0.getAddress()
                << "\n";
      return eld::plugin::Plugin::Status::SUCCESS;
    }
    if (getLinker()->getState() == LinkerWrapper::CreatingSections) {
      for (auto &Section : m_SectionToChunks) {
        for (auto &C : Section.second) {
          addChunkToSection(Section.first, C);
        }
      }
      return eld::plugin::Plugin::Status::SUCCESS;
    }
    return Plugin::Status::SUCCESS;
  }

  void addChunkToSection(std::string Section, Chunk &C) {
    std::vector<eld::plugin::LinkerScriptRule> Rules;

    eld::Expected<eld::plugin::OutputSection> expB = getLinker()->getOutputSection(Section);
    if (!expB.has_value() || !expB.value())
      return;
    eld::plugin::OutputSection B = expB.value();
    eld::plugin::LinkerScriptRule BRule;
    for (auto &R : B.getLinkerScriptRules())
      BRule = R;
    eld::Expected<void> expAddChunk = getLinker()->addChunk(BRule, C);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        getLinker(), expAddChunk);
  }

  void addChunk(std::string Section, Chunk C) {
    m_SectionToChunks[Section].push_back(C);
  }

  void Destroy() override {}

  uint32_t GetLastError() override { return 0; }

  std::string GetLastErrorAsString() override { return "SUCCESS"; }

  std::string GetName() override { return "CHANGESYMBOL"; }

private:
  std::unordered_map<std::string, std::vector<Chunk>> m_SectionToChunks;
};

class DLL_A_EXPORT ChangeSymbolPluginConfig : public LinkerPluginConfig {
public:
  ChangeSymbolPluginConfig(ChangeSymbolPlugin *P)
      : LinkerPluginConfig(P), P(P) {}

  void Init() override {
    std::string b22pcrel = "R_HEX_B22_PCREL";
    eld::Expected<void> expRegReloc =
        P->getLinker()->registerReloc(getRelocationType(b22pcrel));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expRegReloc);
  }

  void RelocCallBack(Use U) override {
    if (P->getLinker()->isMultiThreaded()) {
      std::lock_guard<std::mutex> M(Mutex);
      printMessage(U);
      return;
    }
    printMessage(U);
  }

private:
  void printMessage(Use U) {
    std::cerr << "Got a callback for " << getRelocationName(U.getType())
              << " Payload : " << U.getName() << "\n";
    std::cerr << getPath(U.getTargetChunk().getInputFile()) << "\t"
              << getPath(U.getSourceChunk().getInputFile()) << "\t"
              << U.getOffsetInChunk() << "\n";
    if (U.getName() == "bar")
      changeSymbol(U);
    if (U.getName() == "baz")
      changeSymbolAndaddRelocation(U);
    if (U.getName() == "car")
      changeSymbolCar(U);
  }

  uint32_t getRelocationType(std::string Name) {
    uint32_t rType =
        P->getLinker()->getRelocationHandler().getRelocationType(Name);
    return rType;
  }

  std::string getRelocationName(uint32_t Type) {
    return P->getLinker()->getRelocationHandler().getRelocationName(Type);
  }

  std::string getPath(InputFile I) const {
    std::string FileName = std::string(I.getFileName());
    if (I.isArchive())
      return FileName + "(" + I.getMemberName() + ")";
    return FileName;
  }

  void changeSymbol(eld::plugin::Use &U) {
    const unsigned char NOPBytes[] = {0x00, 0xc0, 0x00, 0x7f};
    const char *Buf = P->getLinker()->getUninitBuffer(sizeof(NOPBytes));
    std::memcpy((void *)Buf, NOPBytes, sizeof(NOPBytes));
    eld::Expected<Chunk> expNOPCode =
        P->getLinker()->createCodeChunk("nop", 4, Buf, sizeof(NOPBytes));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expNOPCode);
    Chunk NOPCode = std::move(expNOPCode.value());

    P->addChunk(".bar", NOPCode);
    eld::Expected<void> expResetSym =
        P->getLinker()->resetSymbol(U.getSymbol(), NOPCode);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expResetSym)
  }

  void changeSymbolAndaddRelocation(eld::plugin::Use &U) {
    const unsigned char callBytes[] = {0x00, 0x40, 0x00, 0x00,  // immext #foo
                                       0x00, 0xc0, 0x00, 0x5a}; // call #foo
    const char *Buf = P->getLinker()->getUninitBuffer(sizeof(callBytes));
    std::memcpy((void *)Buf, callBytes, sizeof(callBytes));
    eld::Expected<Chunk> expCallCode =
        P->getLinker()->createCodeChunk("callfoo", 4, Buf, sizeof(callBytes));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expCallCode);
    Chunk CallCode = std::move(expCallCode.value());

    P->addChunk(".baz", CallCode);
    eld::Expected<void> expResetSym =
        P->getLinker()->resetSymbol(U.getSymbol(), CallCode);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expResetSym)
    uint32_t R_HEX_B32_PCREL_X = getRelocationType("R_HEX_B32_PCREL_X");
    uint32_t R_HEX_B22_PCREL_X = getRelocationType("R_HEX_B22_PCREL_X");
    eld::Expected<eld::plugin::Symbol> expFooSym = P->getLinker()->getSymbol("foo");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expFooSym);
    eld::plugin::Symbol fooSym = std::move(expFooSym.value());

    eld::Expected<eld::plugin::Use> expUse1 =
        P->getLinker()->createAndAddUse(CallCode, 0, R_HEX_B32_PCREL_X, fooSym,
                                        0); // No addend
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expUse1);
    eld::Expected<eld::plugin::Use> expUse2 =
        P->getLinker()->createAndAddUse(CallCode, 4, R_HEX_B22_PCREL_X, fooSym,
                                        4); // Addend = 4
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expUse2);
  }

  void changeSymbolCar(eld::plugin::Use &U) {
    const unsigned char callBytes[] = {0x00, 0xc0, 0x00, 0x5a}; // call #foo
    const char *Buf = P->getLinker()->getUninitBuffer(sizeof(callBytes));
    std::memcpy((void *)Buf, callBytes, sizeof(callBytes));
    eld::Expected<Chunk> expCallCode =
        P->getLinker()->createCodeChunk("carfoo", 4, Buf, sizeof(callBytes));
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expCallCode);
    Chunk CallCode = std::move(expCallCode.value());

    P->addChunk(".car", CallCode);
    eld::Expected<void> expResetSym =
        P->getLinker()->resetSymbol(U.getSymbol(), CallCode);
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expResetSym)
    uint32_t R_HEX_B22_PCREL = getRelocationType("R_HEX_B22_PCREL");
    eld::Expected<eld::plugin::Symbol> expFoo = P->getLinker()->getSymbol("foo");
    ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(
        P->getLinker(), expFoo);
    P->getLinker()->createAndAddUse(CallCode, 0, R_HEX_B22_PCREL,
                                    std::move(expFoo.value()),
                                    0); // No addend
  }

private:
  ChangeSymbolPlugin *P;
  std::mutex Mutex;
};

std::unordered_map<std::string, Plugin *> Plugins;

ChangeSymbolPlugin *ThisPlugin = nullptr;
eld::plugin::LinkerPluginConfig *ThisPluginConfig = nullptr;

extern "C" {
bool DLL_A_EXPORT RegisterAll() {
  ThisPlugin = new ChangeSymbolPlugin();
  ThisPluginConfig = new ChangeSymbolPluginConfig(ThisPlugin);
  return true;
}

PluginBase DLL_A_EXPORT *getPlugin(const char *T) { return ThisPlugin; }

LinkerPluginConfig DLL_A_EXPORT *getPluginConfig(const char *T) {
  return ThisPluginConfig;
}

void DLL_A_EXPORT Cleanup() {
  if (ThisPlugin)
    delete ThisPlugin;
  if (ThisPluginConfig)
    delete ThisPluginConfig;
}
}
