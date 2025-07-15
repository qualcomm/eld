//===- ScriptFile.h--------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_SCRIPTFILE_H
#define ELD_SCRIPT_SCRIPTFILE_H

#include "eld/Script/Assignment.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/ExternCmd.h"
#include "eld/Script/FileToken.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/NameSpec.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/PhdrDesc.h"
#include "eld/Script/RegionAlias.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/VersionScript.h"
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

namespace eld {

class ExcludeFiles;
class Expression;
class GNULDBackend;
class InputBuilder;
class LinkerConfig;
class LinkerScriptFile;
class PhdrsCmd;
class PluginCmd;
class Module;
class OutputSectDesc;
class ScriptCommand;
class SectionsCmd;
class StringList;
class InputFile;
class VersionScript;
class MemoryCmd;
class ScriptSymbol;

/** \class ScriptFile
 *  \brief This class defines the interfaces to a linker script file.
 */

class ScriptFile {
public:
  enum Kind {
    LDScript,              // -T
    ScriptExpression,      // --defsym
    PHDRS,                 // PHDR
    VersionScript,         // --version-script
    DynamicList,           // --dynamic-list
    ExcludeFile,           // EXCLUDE_FILE(...)
    ExternList,            // --extern-list
    DuplicateCodeList,     // --copy-farcalls-from-file
    NoReuseTrampolineList, // -no-reuse-trampolines-file
    Memory,                // MEMORY
    Unknown
  };

  typedef std::vector<ScriptCommand *> CommandQueue;
  typedef CommandQueue::const_iterator const_iterator;
  typedef CommandQueue::iterator iterator;
  typedef CommandQueue::const_reference const_reference;
  typedef CommandQueue::reference reference;

public:
  ScriptFile(Kind PKind, Module &CurModule, LinkerScriptFile &PInput,
             InputBuilder &PBuilder, GNULDBackend &PBackend);

  ~ScriptFile();

  const_iterator begin() const { return LinkerScriptCommandQueue.begin(); }
  iterator begin() { return LinkerScriptCommandQueue.begin(); }
  const_iterator end() const { return LinkerScriptCommandQueue.end(); }
  iterator end() { return LinkerScriptCommandQueue.end(); }

  const_reference front() const { return LinkerScriptCommandQueue.front(); }
  reference front() { return LinkerScriptCommandQueue.front(); }
  const_reference back() const { return LinkerScriptCommandQueue.back(); }
  reference back() { return LinkerScriptCommandQueue.back(); }

  size_t size() const { return LinkerScriptCommandQueue.size(); }

  InputToken *findResolvedFilename(InputToken * Input);

  std::string findIncludeFile(const std::string &Filename, bool &Result,
                              bool State);

  Kind getKind() const { return ScriptFileKind; }

  bool isExternListFile() const { return ScriptFileKind == ExternList; }

  const std::string &name() const { return Name; }
  std::string &name() { return Name; }

  void dump(llvm::raw_ostream &Outs) const;
  eld::Expected<void> activate(Module &CurModule);

  /// ENTRY(symbol)
  ScriptCommand *addEntryPoint(const std::string &Symbol);

  ExternCmd *addExtern(StringList &List);

  // NOCROSSREFS
  void addNoCrossRefs(StringList &List);

  /// OUTPUT_FORMAT(bfdname)
  /// OUTPUT_FORMAT(default, big, little)
  void addOutputFormatCmd(const std::string &PFormat);
  void addOutputFormatCmd(const std::string &PDefault, const std::string &PBig,
                          const std::string &PLittle);

  /// GROUP(file, file, ...)
  /// GROUP(file file ...)
  void addGroupCmd(StringList &PStringList, const Attribute &Attribute);

  /// INPUT(file, file, ...)
  /// INPUT(file file ...)
  void addInputCmd(StringList &PStringList, const Attribute &Attribute);

  /// OUTPUT(filename)
  void addOutputCmd(const std::string &PFileName);

  /// SEARCH_DIR(path)
  void addSearchDirCmd(const std::string &PPath);

  /// OUTPUT_ARCH(bfdarch)
  void addOutputArchCmd(const std::string &PArch);

  /// assignment
  void addAssignment(const std::string &Symbol, Expression *ScriptExpression,
                     Assignment::Type AssignmentType = Assignment::DEFAULT);

  bool linkerScriptHasSectionsCommand() const;

  void enterSectionsCmd();

  void leaveSectionsCmd();

  void enterPhdrsCmd();

  void leavePhdrsCmd();

  void enterOutputSectDesc(const std::string &PName,
                           const OutputSectDesc::Prolog &PProlog);

  void leaveOutputSectDesc(const OutputSectDesc::Epilog &PEpilog);

  void addInputSectDesc(InputSectDesc::Policy PPolicy,
                        const InputSectDesc::Spec &PSpec);

  void addPhdrDesc(const PhdrSpec &Spec);

  void setAsNeeded(bool PEnable = true);

  bool asNeeded() const { return LinkerScriptHasAsNeeded; }

  Module &module() { return ThisModule; }

  /* StringList */
  StringList *createStringList();
  StringList *getCurrentStringList() { return ScriptFileStringList; }

  /* ExcludeFiles */
  ExcludeFiles *createExcludeFiles();
  ExcludeFiles *getCurrentExcludeFiles() { return MPExcludeFiles; }

  /* Exclude Pattern */
  ExcludePattern *createExcludePattern(StrToken *S);

  /* WildcardPattern */
  WildcardPattern *createWildCardPattern(
      StrToken *S, WildcardPattern::SortPolicy P = WildcardPattern::SORT_NONE,
      ExcludeFiles *E = nullptr);

  WildcardPattern *createWildCardPattern(
      llvm::StringRef S,
      WildcardPattern::SortPolicy P = WildcardPattern::SORT_NONE,
      ExcludeFiles *E = nullptr);

  /* ScriptSymbol */
  ScriptSymbol *createScriptSymbol(const StrToken *S) const;

  ScriptSymbol *createScriptSymbol(llvm::StringRef S) const;

  /* Token helpers */
  StrToken *createStrToken(const std::string &);

  FileToken *createFileToken(const std::string &, bool);

  NameSpec *createNameSpecToken(const std::string &, bool);

  StrToken *createParserStr(const char *PText, size_t PLength);

  StrToken *createParserStr(llvm::StringRef S);

  LinkerScriptFile &getLinkerScriptFile() { return ThisLinkerScriptFile; }

  GNULDBackend &backend() { return ThisBackend; }

  /* Identify the first output section inside the first linker script */
  bool firstLinkerScriptWithOutputSection() const {
    return IsFirstLinkerScriptWithSectionCommand;
  }

  PluginCmd *addPlugin(plugin::Plugin::Type T, std::string Name, std::string R,
                       std::string O = "");

  // Support for OUTPUT_ARCH_OPTION.
  void addOutputArchOption(llvm::StringRef Option, Expression *Expr);

  void addOutputArchOptionMap(llvm::StringRef K, llvm::StringRef V);

  // Context
  void setContext(InputFile *);

  InputFile *getContext() const;

  // Adds included linkerscript to --reproduce tarball
  void addInputToTar(const std::string &Filename,
                     const std::string &ResolvedPath) const;

  // Opens scope
  ScriptCommand *enterScope();

  // Exit scope
  ScriptCommand *exitScope();

  // Parent
  void push(eld::ScriptCommand *Cmd) { ScriptCommandStack.push(Cmd); }

  void pop() {
    if (!ScriptCommandStack.size())
      return;
    ScriptCommandStack.pop();
  }

  eld::ScriptCommand *getParent() const {
    if (!ScriptCommandStack.size())
      return nullptr;
    return ScriptCommandStack.top();
  }

  void leavingOutputSectDesc();

  void popScriptStack() { ScriptFileStack.pop(); }

  // Get path of the linker script file.
  llvm::StringRef getPath() const;

  // --------------------- dynamic-list ----------------------------
  std::vector<ScriptSymbol *> *createDynamicList();

  std::vector<ScriptSymbol *> *getDynamicList() const {
    return DynamicListSymbols;
  }

  void addSymbolToDynamicList(ScriptSymbol *S);

  // --------------------- extern-list ----------------------------

  ExternCmd *createExternCmd();

  const StringList &getExternList() {
    return ScriptFileExternCommand->getExternList();
  }

  void addSymbolToExternList(StrToken *S);

  // --------------------- version-script ----------------------------
  eld::VersionScript *getVersionScript();

  eld::VersionScript *createVersionScript();

  // ------------------------ MEMORY ------------------------------------
  void addMemoryRegion(StrToken *Name, StrToken *Attributes, Expression *Origin,
                       Expression *Length);

  void leaveMemoryCmd() {}

  /// This adds support for explicit output section data keywords. They include
  /// BYTE, SHORT, LONG, QUAD, and SQUAD.
  ///
  void addOutputSectData(OutputSectData::OSDKind DataKind, Expression *Expr);

  // ------------------------ REGION_ALIAS ------------------------------------
  void addRegionAlias(const StrToken *Alias, const StrToken *Region);

  // -------------------------- All assignments --------------------------------
  std::vector<Assignment *> getAssignments() { return Assignments; }

  // ----------------- process assignments in link order --------
  void processAssignments();

private:
  Kind ScriptFileKind;
  Module &ThisModule;
  LinkerScriptFile &ThisLinkerScriptFile;
  GNULDBackend &ThisBackend;
  std::string Name;
  CommandQueue LinkerScriptCommandQueue;
  bool LinkerScriptHasSectionsCommand;
  bool ScriptStateInSectionsCommmand;
  bool ScriptStateInsideOutputSection;
  StringList *ScriptFileStringList;
  ExternCmd *ScriptFileExternCommand = nullptr;
  ExcludeFiles *MPExcludeFiles;
  SectionsCmd *LinkerScriptSectionsCommand;
  PhdrsCmd *LinkerScriptPHDRSCommand;
  OutputSectDesc *OutputSectionDescription;
  bool LinkerScriptHasAsNeeded;
  bool ScriptStateInPHDRSCommand;
  static bool IsFirstLinkerScriptWithSectionCommand;
  std::unordered_map<std::string, eld::WildcardPattern *>
      ScriptWildcardPatternMap;
  std::stack<eld::ScriptCommand *> ScriptCommandStack;
  std::stack<InputFile *> ScriptFileStack;
  std::vector<ScriptSymbol *> *DynamicListSymbols = nullptr;
  bool IsLeavingOutputSectDesc = false;
  eld::VersionScript *LinkerVersionScript = nullptr;
  eld::MemoryCmd *MemoryCmd = nullptr;
  std::vector<Assignment *> Assignments;
};

} // namespace eld

#endif
