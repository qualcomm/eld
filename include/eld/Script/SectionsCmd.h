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
  typedef SectionCommands::const_iterator const_iterator;
  typedef SectionCommands::iterator iterator;
  typedef SectionCommands::const_reference const_reference;
  typedef SectionCommands::reference reference;

public:
  SectionsCmd();
  ~SectionsCmd();

  const_iterator begin() const { return ThisSectionCommands.begin(); }
  iterator begin() { return ThisSectionCommands.begin(); }
  const_iterator end() const { return ThisSectionCommands.end(); }
  iterator end() { return ThisSectionCommands.end(); }

  const_reference front() const { return ThisSectionCommands.front(); }
  reference front() { return ThisSectionCommands.front(); }
  const_reference back() const { return ThisSectionCommands.back(); }
  reference back() { return ThisSectionCommands.back(); }

  size_t size() const { return ThisSectionCommands.size(); }

  bool empty() const { return ThisSectionCommands.empty(); }

  void dump(llvm::raw_ostream &Outs) const override;

  void dumpOnlyThis(llvm::raw_ostream &Outs) const override;

  static bool classof(const ScriptCommand *LinkerScriptCommand) {
    return LinkerScriptCommand->getKind() == ScriptCommand::SECTIONS;
  }

  eld::Expected<void> activate(Module &CurModule) override;

  void pushBack(ScriptCommand *PCommand) override;

  SectionCommands &getSectionCommands() { return ThisSectionCommands; }

private:
  SectionCommands ThisSectionCommands;
};

} // namespace eld

#endif
