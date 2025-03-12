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
  ScriptFile(Kind pKind, Module &pModule, LinkerScriptFile &pInput,
             InputBuilder &pBuilder, GNULDBackend &pBackend);

  ~ScriptFile();

  const_iterator begin() const { return m_CommandQueue.begin(); }
  iterator begin() { return m_CommandQueue.begin(); }
  const_iterator end() const { return m_CommandQueue.end(); }
  iterator end() { return m_CommandQueue.end(); }

  const_reference front() const { return m_CommandQueue.front(); }
  reference front() { return m_CommandQueue.front(); }
  const_reference back() const { return m_CommandQueue.back(); }
  reference back() { return m_CommandQueue.back(); }

  size_t size() const { return m_CommandQueue.size(); }

  std::string findIncludeFile(const std::string &filename, bool &result,
                              bool State);

  Kind getKind() const { return m_Kind; }

  bool isExternListFile() const { return m_Kind == ExternList; }

  const std::string &name() const { return m_Name; }
  std::string &name() { return m_Name; }

  void dump(llvm::raw_ostream &outs) const;
  eld::Expected<void> activate(Module &pModule);

  /// ENTRY(symbol)
  ScriptCommand *addEntryPoint(const std::string &pSymbol);

  ExternCmd *addExtern(StringList &pList);

  // NOCROSSREFS
  void addNoCrossRefs(StringList &pList);

  /// OUTPUT_FORMAT(bfdname)
  /// OUTPUT_FORMAT(default, big, little)
  void addOutputFormatCmd(const std::string &pFormat);
  void addOutputFormatCmd(const std::string &pDefault, const std::string &pBig,
                          const std::string &pLittle);

  /// GROUP(file, file, ...)
  /// GROUP(file file ...)
  void addGroupCmd(StringList &pStringList, const Attribute &attribute);

  /// INPUT(file, file, ...)
  /// INPUT(file file ...)
  void addInputCmd(StringList &pStringList, const Attribute &attribute);

  /// OUTPUT(filename)
  void addOutputCmd(const std::string &pFileName);

  /// SEARCH_DIR(path)
  void addSearchDirCmd(const std::string &pPath);

  /// OUTPUT_ARCH(bfdarch)
  void addOutputArchCmd(const std::string &pArch);

  /// assignment
  void addAssignment(const std::string &pSymbol, Expression *pExpr,
                     Assignment::Type pType = Assignment::DEFAULT);

  bool linkerScriptHasSectionsCommand() const;

  void enterSectionsCmd();

  void leaveSectionsCmd();

  void enterPhdrsCmd();

  void leavePhdrsCmd();

  void enterOutputSectDesc(const std::string &pName,
                           const OutputSectDesc::Prolog &pProlog);

  void leaveOutputSectDesc(const OutputSectDesc::Epilog &pEpilog);

  void addInputSectDesc(InputSectDesc::Policy pPolicy,
                        const InputSectDesc::Spec &pSpec);

  void addPhdrDesc(const PhdrSpec &Spec);

  void setAsNeeded(bool pEnable = true);

  bool asNeeded() const { return m_bAsNeeded; }

  Module &module() { return m_Module; }

  /* StringList */
  StringList *createStringList();
  StringList *getCurrentStringList() { return m_pStringList; }

  /* ExcludeFiles */
  ExcludeFiles *createExcludeFiles();
  ExcludeFiles *getCurrentExcludeFiles() { return m_pExcludeFiles; }

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

  StrToken *createParserStr(const char *pText, size_t pLength);

  StrToken *createParserStr(llvm::StringRef s);

  LinkerScriptFile &getLinkerScriptFile() { return m_LinkerScriptFile; }

  GNULDBackend &backend() { return m_Backend; }

  /* Identify the first output section inside the first linker script */
  bool firstLinkerScriptWithOutputSection() const {
    return m_firstLinkerScriptWithOutputSection;
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
  void addInputToTar(const std::string &filename,
                     const std::string &resolvedPath) const;

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

  const StringList &getExternList() { return m_ExternCmd->getExternList(); }

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
  void addOutputSectData(OutputSectData::OSDKind dataKind, Expression *expr);

  // ------------------------ REGION_ALIAS ------------------------------------
  void addRegionAlias(const StrToken *alias, const StrToken *region);

private:
  Kind m_Kind;
  Module &m_Module;
  LinkerScriptFile &m_LinkerScriptFile;
  GNULDBackend &m_Backend;
  std::string m_Name;
  CommandQueue m_CommandQueue;
  bool m_bHasSectionsCmd;
  bool m_bInSectionsCmd;
  bool m_bInOutputSectDesc;
  StringList *m_pStringList;
  ExternCmd *m_ExternCmd = nullptr;
  ExcludeFiles *m_pExcludeFiles;
  SectionsCmd *m_SectionsCmd;
  PhdrsCmd *m_PhdrsCmd;
  OutputSectDesc *m_OutputSectDesc;
  bool m_bAsNeeded;
  bool m_bInPhdrsCmd;
  static bool m_firstLinkerScriptWithOutputSection;
  std::unordered_map<std::string, eld::WildcardPattern *> m_WildcardPatternMap;
  std::stack<eld::ScriptCommand *> ScriptCommandStack;
  std::stack<InputFile *> ScriptFileStack;
  std::vector<ScriptSymbol *> *DynamicListSymbols = nullptr;
  bool m_LeavingOutputSectDesc = false;
  eld::VersionScript *m_VersionScript = nullptr;
  eld::MemoryCmd *m_MemoryCmd = nullptr;
};

} // namespace eld

#endif
