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
ScriptAction::ScriptAction(const std::string &PFileName, ScriptFile::Kind PKind,
                           const LinkerConfig &Config,
                           DiagnosticPrinter *Printer)
    : InputFileAction(PFileName, InputAction::Script, Printer),
      ScriptFileKind(PKind), ThisConfig(Config) {}

bool ScriptAction::activate(InputBuilder &PBuilder) {
  std::string Path = Name;
  auto &SearchDirs = ThisConfig.directories();
  if (!llvm::sys::fs::exists(Path)) {
    const sys::fs::Path *Res = SearchDirs.find(Path, Input::Script);
    if (Res == nullptr) {
      switch (ScriptFileKind) {
      case ScriptFile::LDScript:
        ThisConfig.raise(Diag::err_cannot_find_scriptfile)
            << "linker script" << Path;
        break;
      case ScriptFile::VersionScript:
        ThisConfig.raise(Diag::err_cannot_find_scriptfile)
            << "version script" << Path;
        break;
      case ScriptFile::DynamicList:
        ThisConfig.raise(Diag::err_cannot_find_scriptfile)
            << "dynamic list" << Path;
        break;
      default:
        break;
      }
      return false;
    }
    Path = Res->native();
  }
  setFileName(Path);
  InputFileAction::activate(PBuilder);

  // Resolve the path so that the appropriate file has been read and the memory
  // area created for it.
  if (!I->resolvePath(ThisConfig))
    return false;

  // Create an InputFile and set the Input back.
  LinkerScriptFile *LSFile =
      make<eld::LinkerScriptFile>(I, ThisConfig.getDiagEngine());
  I->setInputFile(LSFile);

  return true;
}
