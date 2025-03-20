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

  ScriptCommand(Kind PKind)
      : ScriptFileKind(PKind), ParentScriptCommand(nullptr) {}

  virtual ~ScriptCommand() = 0;

  virtual void dump(llvm::raw_ostream &Outs) const = 0;

  virtual void dumpMap(llvm::raw_ostream &Out, bool Color = false,
                       bool EndWithNewLine = true, bool WithValues = false,
                       bool AddIndent = true) const;

  virtual void dumpOnlyThis(llvm::raw_ostream &Outs) const {
    doIndent(Outs);
    dump(Outs);
  }

  virtual eld::Expected<void> activate(Module &) = 0;

  Kind getKind() const { return ScriptFileKind; }

  /// ----------------- Extra Informative Context --------------
  void setInputFileInContext(InputFile *File) { ThisScriptFile = File; }

  void setLineNumberInContext(size_t Context) { LineNumber = Context; }

  bool hasInputFileInContext() const { return ThisScriptFile; }

  InputFile *getInputFileInContext() const { return ThisScriptFile; }

  bool hasLineNumberInContext() const { return LineNumber.has_value(); }

  size_t getLineNumberInContext() const { return *LineNumber; }

  std::string getContext() const;

  /// -------------------- Parent Information --------------------
  virtual uint32_t getDepth() const;

  void setParent(ScriptCommand *Parent) { ParentScriptCommand = Parent; }

  ScriptCommand *getParent() const { return ParentScriptCommand; }

  void doIndent(llvm::raw_ostream &Outs) const;

  /// ------------------Helper functions---------------------------

  bool isAssert() const { return ScriptFileKind == ASSERT; }

  bool isAssignment() const { return ScriptFileKind == ASSIGNMENT; }

  bool isEnterScope() const { return ScriptFileKind == ENTER_SCOPE; }

  bool isExitScope() const { return ScriptFileKind == EXIT_SCOPE; }

  bool isEntry() const { return ScriptFileKind == ENTRY; }

  bool isExtern() const { return ScriptFileKind == EXTERN; }

  bool isGroup() const { return ScriptFileKind == GROUP; }

  bool isInput() const { return ScriptFileKind == INPUT; }

  bool isRuleContainer() const { return ScriptFileKind == INPUT_SECT_DESC; }

  bool isNoCrossRefs() const { return ScriptFileKind == NOCROSSREFS; }

  bool isOutput() const { return ScriptFileKind == OUTPUT; }

  bool isOutputArch() const { return ScriptFileKind == OUTPUT_ARCH; }

  bool isOutputFormat() const { return ScriptFileKind == OUTPUT_FORMAT; }

  bool isOutputSectionDescription() const {
    return ScriptFileKind == OUTPUT_SECT_DESC;
  }

  bool isPhdrDesc() const { return ScriptFileKind == PHDR_DESC; }

  bool isPhdrs() const { return ScriptFileKind == PHDRS; }

  bool isPlugin() const { return ScriptFileKind == PLUGIN; }

  bool isSearchDir() const { return ScriptFileKind == SEARCH_DIR; }

  bool isSections() const { return ScriptFileKind == SECTIONS; }

  bool isMemoryDesc() const { return ScriptFileKind == MEMORY_DESC; }

  virtual void pushBack(ScriptCommand *) {}

private:
  Kind ScriptFileKind;
  InputFile *ThisScriptFile = nullptr;
  // m_LineNum represents the line number corresponding to the script command in
  // m_ScriptFile
  std::optional<size_t> LineNumber;
  ScriptCommand *ParentScriptCommand;
};

} // namespace eld

#endif
