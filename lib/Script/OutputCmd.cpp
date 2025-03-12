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
OutputCmd::OutputCmd(const std::string &pOutputFile)
    : ScriptCommand(ScriptCommand::OUTPUT), m_OutputFile(pOutputFile) {}

OutputCmd::~OutputCmd() {}

void OutputCmd::dump(llvm::raw_ostream &outs) const {
  outs << "OUTPUT(" << m_OutputFile << ")\n";
}

eld::Expected<void> OutputCmd::activate(Module &pModule) {
  GeneralOptions &options = pModule.getConfig().options();
  // Sets output filename if an output filename is not specified using
  // '-o|--output' linker command-line options.
  if (!options.hasOutputFileName()) {
    options.setOutputFileName(m_OutputFile);
    pModule.getConfig().raise(diag::verbose_linker_script_output_file)
        << m_OutputFile << getInputFileInContext()->getInput()->decoratedPath();
  }
  return eld::Expected<void>();
}
