//===- LSParserVerifier.cpp------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "eld/Config/LinkerConfig.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Input/Input.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Script/ScriptFile.h"
#include "eld/Script/ScriptReader.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/Target/GNULDBackend.h"
#include "eld/Target/TargetInfo.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstring>
#include <memory>

class CommonTargetInfo : public eld::TargetInfo {
public:
  CommonTargetInfo(eld::LinkerConfig &config) : eld::TargetInfo(config) {}

  uint64_t flags() const override { return 0; }

  uint32_t machine() const override { return 0; }

  std::string getMachineStr() const override { return ""; }

  uint64_t startAddr(bool linkerScriptHasSectionsCommand, bool isDynExec,
                     bool loadPhdr) const override {
    return 0;
  }
};

class CommonLDBackend : public eld::GNULDBackend {
public:
  CommonLDBackend(eld::Module &module, eld::TargetInfo *info)
      : GNULDBackend(module, info) {}

  bool finalizeTargetSymbols() override { return false; }
  eld::Relocator *getRelocator() const override { return nullptr; }
  bool initRelocator() override { return false; }
  void initTargetSections(eld::ObjectBuilder &pBuilder) override { return; }
  void initTargetSymbols() override { return; }
  size_t getRelEntrySize() override { return 0; }
  size_t getRelaEntrySize() override { return 0; }
  eld::ELFDynamic *dynamic() override { return nullptr; }
  std::size_t PLTEntriesCount() const override { return 0; }
  std::size_t GOTEntriesCount() const override { return 0; }
  eld::Stub *getBranchIslandStub(eld::Relocation *pReloc,
                                 int64_t pTargetValue) const override {
    return nullptr;
  }
};

class LinkerModel {
  eld::DiagnosticEngine m_DiagEngine;
  eld::LinkerConfig *m_Config = nullptr;
  eld::LinkerScript *m_LinkerScript = nullptr;
  eld::Module *m_Module = nullptr;
  eld::GNULDBackend *m_Backend = nullptr;
  CommonTargetInfo *targetInfo = nullptr;
  eld::ScriptReader m_ScriptReader;
  eld::IRBuilder *m_Builder = nullptr;

public:
  LinkerModel() : m_DiagEngine(/*useColor=*/false) {}

  ~LinkerModel() {}

  void Initialize() {
    m_Config = eld::make<eld::LinkerConfig>(&m_DiagEngine);
    std::unique_ptr<eld::DiagnosticInfos> diagInfo =
        std::make_unique<eld::DiagnosticInfos>(*m_Config);
    m_DiagEngine.setInfoMap(std::move(diagInfo));
    m_LinkerScript = eld::make<eld::LinkerScript>(&m_DiagEngine);
    m_Module = eld::make<eld::Module>(*m_LinkerScript, *m_Config,
                                      /*layoutInfo=*/nullptr);
    targetInfo = eld::make<CommonTargetInfo>(*m_Config);
    m_Backend = eld::make<CommonLDBackend>(*m_Module, targetInfo);
    m_Builder = eld::make<eld::IRBuilder>(*m_Module, *m_Config);
  }

  eld::LinkerScriptFile *CreateLinkerScriptFile(const std::string &filename) {
    eld::Input *I = eld::make<eld::Input>(filename, &m_DiagEngine);
    if (!I->resolvePath(*m_Config))
      return nullptr;
    eld::InputFile *IF = eld::InputFile::create(I, &m_DiagEngine);
    I->setInputFile(IF);
    return llvm::cast<eld::LinkerScriptFile>(IF);
  }

  eld::ScriptFile CreateScriptFile(const std::string &filename) {
    eld::LinkerScriptFile *IF = CreateLinkerScriptFile(filename);
    assert(IF && "IF must not be null!!");
    return eld::ScriptFile(eld::ScriptFile::LDScript, *m_Module, *IF,
                           m_Builder->getInputBuilder(), *m_Backend);
  }

  eld::LinkerConfig *getConfig() { return m_Config; }

  eld::ScriptReader &getScriptReader() { return m_ScriptReader; }
};

int main(int argc, char **argv) {
  LinkerModel linkerModel;
  linkerModel.Initialize();
  std::string filename;
  eld::LinkerConfig &config = *linkerModel.getConfig();
  for (int i = 1; i < argc; ++i)
    filename = argv[i];
  eld::ScriptFile SF = linkerModel.CreateScriptFile(filename);
  bool res = linkerModel.getScriptReader().readScript(config, SF);
  return res == 0;
}
