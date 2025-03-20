//===- OutputCmd.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/OutputCmd.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/SymbolResolver/IRBuilder.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// OutputCmd
//===----------------------------------------------------------------------===//
OutputCmd::OutputCmd(const std::string &POutputFile)
    : ScriptCommand(ScriptCommand::OUTPUT), OutputFileName(POutputFile) {}

OutputCmd::~OutputCmd() {}

void OutputCmd::dump(llvm::raw_ostream &Outs) const {
  Outs << "OUTPUT(" << OutputFileName << ")\n";
}

eld::Expected<void> OutputCmd::activate(Module &CurModule) {
  GeneralOptions &Options = CurModule.getConfig().options();
  // Sets output filename if an output filename is not specified using
  // '-o|--output' linker command-line options.
  if (!Options.hasOutputFileName()) {
    Options.setOutputFileName(OutputFileName);
    CurModule.getConfig().raise(Diag::verbose_linker_script_output_file)
        << OutputFileName
        << getInputFileInContext()->getInput()->decoratedPath();
  }
  return eld::Expected<void>();
}
