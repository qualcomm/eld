//===- SectionsCmd.h-------------------------------------------------------===//
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
#ifndef ELD_SCRIPT_SECTIONSCMD_H
#define ELD_SCRIPT_SECTIONSCMD_H

#include "eld/Script/ScriptCommand.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/DataTypes.h"
#include <vector>

namespace eld {

class Module;

/** \class SectionsCmd
 *  \brief This class defines the interfaces to SECTIONS command.
 */

class SectionsCmd : public ScriptCommand {
public:
  typedef std::vector<ScriptCommand *> SectionCommands;
  using SectionCmdRange = llvm::iterator_range<SectionCommands::iterator>;
  using SectionCmdConstRange =
      llvm::iterator_range<SectionCommands::const_iterator>;

public:
  SectionsCmd();

  SectionCmdRange commands() {
    return llvm::make_range(ThisSectionCommands.begin(),
                            ThisSectionCommands.end());
  }
  SectionCmdConstRange commands() const {
    return llvm::make_range(ThisSectionCommands.begin(),
                            ThisSectionCommands.end());
  }

  const ScriptCommand *front() const { return ThisSectionCommands.front(); }
  ScriptCommand *front() { return ThisSectionCommands.front(); }
  const ScriptCommand *back() const { return ThisSectionCommands.back(); }
  ScriptCommand *back() { return ThisSectionCommands.back(); }

  size_t size() const { return ThisSectionCommands.size(); }

  bool empty() const { return ThisSectionCommands.empty(); }

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::SECTIONS;
  }

  eld::Expected<void> activate(Module &CurModule) override;

  void pushBack(ScriptCommand *PCommand) override;

private:
  SectionCommands ThisSectionCommands;
};

} // namespace eld

#endif
