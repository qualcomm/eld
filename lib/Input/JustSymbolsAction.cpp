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
JustSymbolsAction::JustSymbolsAction(const std::string &fileName,
                                     const LinkerConfig &config,
                                     DiagnosticPrinter *diagPrinter)
    : InputFileAction(fileName, InputAction::JustSymbols, diagPrinter),
      m_Config(config) {}

bool JustSymbolsAction::activate(InputBuilder &pBuilder) {
  InputFileAction::activate(pBuilder);
  I->getAttribute().setJustSymbols();
  m_Config.raise(diag::verbose_using_just_symbols) << I->getFileName();
  return true;
}
