//===- AdvancedLTO.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "Defines.h"
#include "LinkerPlugin.h"
#include "LinkerWrapper.h"
#include "PluginADT.h"
#include "PluginVersion.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/LTO/LTO.h"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

namespace {

using namespace eld::plugin;

struct ModuleData {
  explicit ModuleData(BitcodeFile BCF) : BCFile(BCF) {}

  BitcodeFile BCFile;
  std::unordered_map<std::string, Section> SectionsByName;
};

class DLL_A_EXPORT AdvancedLTO : public LinkerPlugin {
public:
  AdvancedLTO() : LinkerPlugin("AdvancedLTO") {}

  void Init(const std::string &Options) override {
    (void)Options;
    EnableLTOLinkerScripts =
        getLinker()->getLinkerConfig().hasLTOLinkerScripts();
    TraceLTO = getLinker()->getLinkerConfig().shouldTraceLTO();
  }

  LTOModule *CreateLTOModule(BitcodeFile BCF, LTOModuleHash Hash) override {
    auto Data = std::make_unique<ModuleData>(BCF);
    LTOModule *M = reinterpret_cast<LTOModule *>(Data.get());
    Modules[M] = std::move(Data);
    FilesByHash.insert_or_assign(Hash, &BCF.getBitcodeFile());
    return M;
  }

  void ReadSymbols(LTOModule &M) override {
    ModuleData *Data = importModule(M);
    if (!Data)
      return;

    auto getKind = [](const llvm::lto::InputFile::Symbol &Sym) {
      if (Sym.isUndefined())
        return Symbol::Undefined;
      if (Sym.isCommon())
        return Symbol::Common;
      return Symbol::Define;
    };

    auto getBinding = [](const llvm::lto::InputFile::Symbol &Sym) {
      if (!Sym.isGlobal())
        return Symbol::Local;
      if (Sym.isWeak())
        return Symbol::Weak;
      return Symbol::Global;
    };

    auto getVisibility = [](const llvm::lto::InputFile::Symbol &Sym) {
      switch (Sym.getVisibility()) {
      case llvm::GlobalValue::DefaultVisibility:
        return Symbol::Default;
      case llvm::GlobalValue::HiddenVisibility:
        return Symbol::Hidden;
      case llvm::GlobalValue::ProtectedVisibility:
        return Symbol::Protected;
      }
      return Symbol::Default;
    };

    auto getType = [](const llvm::lto::InputFile::Symbol &Sym) {
      if (Sym.isExecutable())
        return static_cast<unsigned>(llvm::ELF::STT_FUNC);
      if (Sym.isTLS())
        return static_cast<unsigned>(llvm::ELF::STT_TLS);
      return static_cast<unsigned>(llvm::ELF::STT_OBJECT);
    };

    llvm::ArrayRef<llvm::lto::InputFile::Symbol> Symbols =
        Data->BCFile.getInputFile().symbols();

    for (const auto &Sym : llvm::enumerate(Symbols)) {
      unsigned SymbolIndex = Sym.index();
      const llvm::lto::InputFile::Symbol &LtoSym = Sym.value();
      bool KeepComdat = Data->BCFile.findIfKeptComdat(LtoSym.getComdatIndex());
      Symbol::Kind Kind = KeepComdat ? getKind(LtoSym) : Symbol::Undefined;

      Section InputSection;
      llvm::StringRef SectionName = LtoSym.getSectionName();
      if (SectionName.empty() && LtoSym.isCommon())
        SectionName = "COMMON";

      if (!SectionName.empty()) {
        auto ExpSection = getOrCreateSection(*Data, SectionName);
        ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), ExpSection);
        InputSection = *ExpSection;
      }

      auto ExpSymbol = getLinker()->addSymbol(
          Data->BCFile, LtoSym.getName().str(), getBinding(LtoSym),
          InputSection, Kind, getVisibility(LtoSym), getType(LtoSym),
          LtoSym.isCommon() ? LtoSym.getCommonSize() : 0, SymbolIndex);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), ExpSymbol);

      if (!EnableLTOLinkerScripts || !InputSection || Kind == Symbol::Undefined)
        continue;

      auto Rule = InputSection.getLinkerScriptRule();
      if (!Rule || !Rule.isKeep())
        continue;

      auto ExpSetPreserve = getLinker()->setPreserveSymbol(*ExpSymbol);
      ELDEXP_REPORT_AND_RETURN_VOID_IF_ERROR(getLinker(), ExpSetPreserve);
    }
  }

  void VisitSections(InputFile IF) override {
    if (!EnableLTOLinkerScripts)
      return;

    for (Section S : IF.getSections()) {
      std::string Name = S.getName();
      size_t FirstPos = Name.find("^^");
      if (FirstPos == std::string::npos)
        continue;
      std::string BaseName = Name.substr(0, FirstPos);
      std::string HashStr = Name.substr(FirstPos + 2);
      size_t SecondPos = HashStr.find("^^");
      if (SecondPos != std::string::npos)
        HashStr = HashStr.substr(SecondPos + 2);
      if (HashStr.empty())
        continue;

      if (!getLinker()->isPostLTOPhase())
        continue;

      errno = 0;
      char *End = nullptr;
      unsigned long long Parsed = std::strtoull(HashStr.c_str(), &End, 16);
      if (errno != 0 || End == HashStr.c_str() || *End != '\0')
        continue;
      uint64_t Hash = Parsed;

      auto It = FilesByHash.find(Hash);
      if (It == FilesByHash.end())
        continue;

      getLinker()->setSectionName(S, BaseName);
      getLinker()->setRuleMatchingInput(S, BitcodeFile(*It->second));
      if (TraceLTO) {
        (void)getLinker()->reportDiag(
            getLinker()->getNoteDiagID(
                "Applying section namespace override %0"),
            Name);
      }
    }
  }

private:
  ModuleData *importModule(LTOModule &M) {
    auto *ID = &M;
    auto It = Modules.find(ID);
    if (It == Modules.end())
      return nullptr;
    return It->second.get();
  }

  Expected<Section> getOrCreateSection(ModuleData &Data,
                                       llvm::StringRef SectionName) {
    std::string Key = SectionName.str();
    auto It = Data.SectionsByName.find(Key);
    if (It != Data.SectionsByName.end())
      return It->second;

    auto ExpSection = getLinker()->createBitcodeSection(Key, Data.BCFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(ExpSection);
    Data.SectionsByName.emplace(std::move(Key), *ExpSection);
    return *ExpSection;
  }

private:
  bool EnableLTOLinkerScripts = false;
  bool TraceLTO = false;
  std::unordered_map<LTOModule *, std::unique_ptr<ModuleData>> Modules;
  std::unordered_map<uint64_t, eld::BitcodeFile *> FilesByHash;
};

} // namespace

ELD_REGISTER_PLUGIN(AdvancedLTO)
