//===- JustSymbolsAction.h-------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_INPUT_JUSTSYMBOLSACTION_H
#define ELD_INPUT_JUSTSYMBOLSACTION_H

#include "eld/Input/InputAction.h"

namespace eld {

class LinkerConfig;

/// JustSymbolsAction
class JustSymbolsAction : public InputFileAction {
public:
  JustSymbolsAction(const std::string &FileName, const LinkerConfig &Config,
                    DiagnosticPrinter *DiagPrinter);

  bool activate(InputBuilder &) override;

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const JustSymbolsAction *S) { return true; }

private:
  const LinkerConfig &Config;
};
} // namespace eld

#endif
