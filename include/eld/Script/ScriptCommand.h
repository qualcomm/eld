//===- ScriptCommand.h-----------------------------------------------------===//
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

#ifndef ELD_SCRIPT_SCRIPTCOMMAND_H
#define ELD_SCRIPT_SCRIPTCOMMAND_H

#include "eld/PluginAPI/Expected.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace eld {

class Module;
class InputFile;

/** \class ScriptCommand
 *  \brief This class defines the interfaces to a script command.
 */
class ScriptCommand {
public:
  enum Kind {
    ASSERT,
    ASSIGNMENT,
    ENTER_SCOPE,
    EXIT_SCOPE,
    ENTRY,
    EXTERN,
    GROUP,
    INCLUDE,
    INPUT,
    INPUT_SECT_DESC,
    NOCROSSREFS,
    OUTPUT,
    OUTPUT_ARCH,
    OUTPUT_FORMAT,
    OUTPUT_SECT_DESC,
    OUTPUT_SECT_DATA,
    PHDR_DESC,
    PHDRS,
    PLUGIN,
    SEARCH_DIR,
    SECTIONS,
    MEMORY,
    MEMORY_DESC,
    REGION_ALIAS
  };

  ScriptCommand(Kind pKind) : m_Kind(pKind), m_Parent(nullptr) {}

  virtual ~ScriptCommand() = 0;

  virtual void dump(llvm::raw_ostream &outs) const = 0;

  virtual void dumpMap(llvm::raw_ostream &out, bool color = false,
                       bool endWithNewLine = true, bool withValues = false,
                       bool addIndent = true) const;

  virtual void dumpOnlyThis(llvm::raw_ostream &outs) const {
    doIndent(outs);
    dump(outs);
  }

  virtual eld::Expected<void> activate(Module &) = 0;

  Kind getKind() const { return m_Kind; }

  /// ----------------- Extra Informative Context --------------
  void setInputFileInContext(InputFile *file) { m_ScriptFile = file; }

  void setLineNumberInContext(size_t context) { m_LineNum = context; }

  bool hasInputFileInContext() const { return m_ScriptFile; }

  InputFile *getInputFileInContext() const { return m_ScriptFile; }

  bool hasLineNumberInContext() const { return m_LineNum.has_value(); }

  size_t getLineNumberInContext() const { return *m_LineNum; }

  std::string getContext() const;

  /// -------------------- Parent Information --------------------
  virtual uint32_t getDepth() const;

  void setParent(ScriptCommand *Parent) { m_Parent = Parent; }

  ScriptCommand *getParent() const { return m_Parent; }

  void doIndent(llvm::raw_ostream &outs) const;

  /// ------------------Helper functions---------------------------

  bool isAssert() const { return m_Kind == ASSERT; }

  bool isAssignment() const { return m_Kind == ASSIGNMENT; }

  bool isEnterScope() const { return m_Kind == ENTER_SCOPE; }

  bool isExitScope() const { return m_Kind == EXIT_SCOPE; }

  bool isEntry() const { return m_Kind == ENTRY; }

  bool isExtern() const { return m_Kind == EXTERN; }

  bool isGroup() const { return m_Kind == GROUP; }

  bool isInput() const { return m_Kind == INPUT; }

  bool isRuleContainer() const { return m_Kind == INPUT_SECT_DESC; }

  bool isNoCrossRefs() const { return m_Kind == NOCROSSREFS; }

  bool isOutput() const { return m_Kind == OUTPUT; }

  bool isOutputArch() const { return m_Kind == OUTPUT_ARCH; }

  bool isOutputFormat() const { return m_Kind == OUTPUT_FORMAT; }

  bool isOutputSectionDescription() const { return m_Kind == OUTPUT_SECT_DESC; }

  bool isPhdrDesc() const { return m_Kind == PHDR_DESC; }

  bool isPhdrs() const { return m_Kind == PHDRS; }

  bool isPlugin() const { return m_Kind == PLUGIN; }

  bool isSearchDir() const { return m_Kind == SEARCH_DIR; }

  bool isSections() const { return m_Kind == SECTIONS; }

  bool isMemoryDesc() const { return m_Kind == MEMORY_DESC; }

  virtual void push_back(ScriptCommand *) {}

private:
  Kind m_Kind;
  InputFile *m_ScriptFile = nullptr;
  // m_LineNum represents the line number corresponding to the script command in
  // m_ScriptFile
  std::optional<size_t> m_LineNum;
  ScriptCommand *m_Parent;
};

} // namespace eld

#endif
