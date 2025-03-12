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
  typedef FormatList::const_iterator const_iterator;
  typedef FormatList::iterator iterator;

public:
  OutputFormatCmd(const std::string &pFormat);
  OutputFormatCmd(const std::string &pDefault, const std::string &pBig,
                  const std::string &pLittle);
  ~OutputFormatCmd();

  const_iterator begin() const { return m_FormatList.begin(); }
  iterator begin() { return m_FormatList.begin(); }
  const_iterator end() const { return m_FormatList.end(); }
  iterator end() { return m_FormatList.end(); }

  void dump(llvm::raw_ostream &outs) const override;

  static bool classof(const ScriptCommand *pCmd) {
    return pCmd->getKind() == ScriptCommand::OUTPUT_FORMAT;
  }

  eld::Expected<void> activate(Module &pModule) override;

private:
  FormatList m_FormatList;
};

} // namespace eld

#endif
