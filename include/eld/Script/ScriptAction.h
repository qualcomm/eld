//===- ScriptAction.h------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_SCRIPTACTION_H
#define ELD_SCRIPT_SCRIPTACTION_H

#include "eld/Input/InputAction.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Script/ScriptFile.h"

namespace eld {

class LinkerConfig;

/// ScriptAction
class ScriptAction : public InputFileAction {
public:
  ScriptAction(const std::string &PFileName, ScriptFile::Kind PKind,
               const LinkerConfig &PConfig, DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  const std::string &filename() const { return FileNamePatternName; }

  ScriptFile::Kind kind() const { return ScriptFileKind; }

  LinkerScriptFile *getLinkerScriptFile() const {
    return llvm::dyn_cast<eld::LinkerScriptFile>(I->getInputFile());
  }

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const ScriptAction *S) { return true; }

private:
  std::string FileNamePatternName;
  ScriptFile::Kind ScriptFileKind;
  const LinkerConfig &ThisConfig;
};
} // namespace eld

#endif
