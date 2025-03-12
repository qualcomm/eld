//===- ScriptAction.cpp----------------------------------------------------===//
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

//===- ScriptAction.cpp- --------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Script/ScriptAction.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/FileSystem.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ScriptAction
//===----------------------------------------------------------------------===//
ScriptAction::ScriptAction(const std::string &pFileName, ScriptFile::Kind pKind,
                           const LinkerConfig &Config,
                           DiagnosticPrinter *Printer)
    : InputFileAction(pFileName, InputAction::Script, Printer), m_Kind(pKind),
      m_Config(Config) {}

bool ScriptAction::activate(InputBuilder &pBuilder) {
  std::string Path = m_Name;
  auto &searchDirs = m_Config.directories();
  if (!llvm::sys::fs::exists(Path)) {
    const sys::fs::Path *res = searchDirs.find(Path, Input::Script);
    if (res == nullptr) {
      switch (m_Kind) {
      case ScriptFile::LDScript:
        m_Config.raise(diag::err_cannot_find_scriptfile)
            << "linker script" << Path;
        break;
      case ScriptFile::VersionScript:
        m_Config.raise(diag::err_cannot_find_scriptfile)
            << "version script" << Path;
        break;
      case ScriptFile::DynamicList:
        m_Config.raise(diag::err_cannot_find_scriptfile)
            << "dynamic list" << Path;
        break;
      default:
        break;
      }
      return false;
    }
    Path = res->native();
  }
  setFileName(Path);
  InputFileAction::activate(pBuilder);

  // Resolve the path so that the appropriate file has been read and the memory
  // area created for it.
  if (!I->resolvePath(m_Config))
    return false;

  // Create an InputFile and set the Input back.
  LinkerScriptFile *LSFile =
      make<eld::LinkerScriptFile>(I, m_Config.getDiagEngine());
  I->setInputFile(LSFile);

  return true;
}
