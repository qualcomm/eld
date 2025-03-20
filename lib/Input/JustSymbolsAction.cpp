//===- JustSymbolsAction.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/JustSymbolsAction.h"
#include "eld/Config/LinkerConfig.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// JustSymbolsAction
//===----------------------------------------------------------------------===//
JustSymbolsAction::JustSymbolsAction(const std::string &FileName,
                                     const LinkerConfig &Config,
                                     DiagnosticPrinter *DiagPrinter)
    : InputFileAction(FileName, InputAction::JustSymbols, DiagPrinter),
      Config(Config) {}

bool JustSymbolsAction::activate(InputBuilder &PBuilder) {
  InputFileAction::activate(PBuilder);
  I->getAttribute().setJustSymbols();
  Config.raise(Diag::verbose_using_just_symbols) << I->getFileName();
  return true;
}
