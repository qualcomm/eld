//===- OutputFormatCmd.h---------------------------------------------------===//
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
#ifndef ELD_SCRIPT_OUTPUTFORMATCMD_H
#define ELD_SCRIPT_OUTPUTFORMATCMD_H

#include "eld/Script/ScriptCommand.h"
#include "llvm/ADT/iterator_range.h"
#include <string>
#include <vector>

namespace eld {

class Module;

/** \class OutputFormatCmd
 *  \brief This class defines the interfaces to OutputFormat command.
 */

class OutputFormatCmd : public ScriptCommand {
public:
  typedef std::vector<std::string> FormatList;
  using FormatRange = llvm::iterator_range<FormatList::iterator>;
  using FormatConstRange = llvm::iterator_range<FormatList::const_iterator>;

public:
  OutputFormatCmd(const std::string &PFormat);
  OutputFormatCmd(const std::string &PDefault, const std::string &PBig,
                  const std::string &PLittle);

  FormatRange formats() {
    return llvm::make_range(OutputFormatList.begin(), OutputFormatList.end());
  }
  FormatConstRange formats() const {
    return llvm::make_range(OutputFormatList.begin(), OutputFormatList.end());
  }

  void dump(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::OUTPUT_FORMAT;
  }

  eld::Expected<void> activate(Module &CurModule) override;

private:
  FormatList OutputFormatList;
};

} // namespace eld

#endif
