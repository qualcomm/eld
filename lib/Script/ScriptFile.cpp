//===- ScriptFile.cpp------------------------------------------------------===//
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

#include "eld/Script/ScriptFile.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELDDirectory.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/LinkerScriptFile.h"
#include "eld/Input/SearchDirs.h"
#include "eld/Script/EnterScopeCmd.h"
#include "eld/Script/EntryCmd.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/ExitScopeCmd.h"
#include "eld/Script/Expression.h"
#include "eld/Script/ExternCmd.h"
#include "eld/Script/FileToken.h"
#include "eld/Script/GroupCmd.h"
#include "eld/Script/IncludeCmd.h"
#include "eld/Script/InputCmd.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/MemoryCmd.h"
#include "eld/Script/NoCrossRefsCmd.h"
#include "eld/Script/OutputArchCmd.h"
#include "eld/Script/OutputCmd.h"
#include "eld/Script/OutputFormatCmd.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/PhdrsCmd.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Script/SearchDirCmd.h"
#include "eld/Script/SectionsCmd.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "eld/Script/VersionScript.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/MappingFile.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Target/GNULDBackend.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include <string>

using namespace eld;

static eld::StringList *oldEpilogPhdrs = nullptr;
static bool hasEpilogPhdrs = false;
bool ScriptFile::m_firstLinkerScriptWithOutputSection = false;

//===----------------------------------------------------------------------===//
// ScriptFile
//===----------------------------------------------------------------------===//
ScriptFile::ScriptFile(Kind pKind, Module &pModule, LinkerScriptFile &pInput,
                       InputBuilder &pBuilder, GNULDBackend &pBackend)
    : m_Kind(pKind), m_Module(pModule), m_LinkerScriptFile(pInput),
      m_Backend(pBackend),
      m_Name(pInput.getInput()->getResolvedPath().native()),
      m_bHasSectionsCmd(false), m_bInSectionsCmd(false),
      m_bInOutputSectDesc(false), m_pStringList(nullptr),
      m_SectionsCmd(nullptr), m_PhdrsCmd(nullptr), m_OutputSectDesc(nullptr),
      m_bAsNeeded(false) {
  setContext(&pInput);
}

ScriptFile::~ScriptFile() {}

void ScriptFile::dump(llvm::raw_ostream &outs) const {
  for (const auto &elem : *this)
    (elem)->dump(outs);
}

eld::Expected<void> ScriptFile::activate(Module &pModule) {
  for (auto &SC : *this) {
    eld::Expected<void> E = SC->activate(pModule);
    if (!E)
      return E;
    // There can be multiple scripts included and the linker needs to be parse
    // each one of them.
    pModule.getScript().addScriptCommand(SC);
  }
  if (m_Kind == Kind::DynamicList && DynamicListSymbols) {
    for (ScriptSymbol *sym : *DynamicListSymbols)
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(sym->activate());
  }
  return eld::Expected<void>();
}

ScriptCommand *ScriptFile::addEntryPoint(const std::string &pSymbol) {
  auto entry = make<EntryCmd>(pSymbol);
  entry->setInputFileInContext(getContext());
  entry->setParent(getParent());

  if (m_bInSectionsCmd) {
    m_SectionsCmd->push_back(entry);
  } else {
    m_CommandQueue.push_back(entry);
  }
  return entry;
}

ExternCmd *ScriptFile::addExtern(StringList &pList) {
  auto externCmd = make<ExternCmd>(pList);
  externCmd->setInputFileInContext(getContext());
  externCmd->setParent(getParent());
  m_CommandQueue.push_back(externCmd);
  return externCmd;
}

void ScriptFile::addNoCrossRefs(StringList &pList) {
  auto noCrossRefs = make<NoCrossRefsCmd>(pList, m_CommandQueue.size());
  noCrossRefs->setInputFileInContext(getContext());
  noCrossRefs->setParent(getParent());
  m_CommandQueue.push_back(noCrossRefs);
}

void ScriptFile::addInputToTar(const std::string &filename,
                               const std::string &resolvedPath) const {
  if (!m_Module.getOutputTarWriter())
    return;
  m_Module.getOutputTarWriter()->createAndAddScriptFile(filename, resolvedPath);
}

namespace {

inline bool searchIncludeFile(llvm::StringRef Name, llvm::StringRef FileName,
                              DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(diag::verbose_trying_script_include_file)
      << FileName << Name << (Found ? "found" : "not found");
  return Found;
}

} // namespace

std::string ScriptFile::findIncludeFile(const std::string &filename,
                                        bool &result, bool state) {
  LinkerConfig &Config = m_Module.getConfig();
  auto iterb = Config.directories().begin();
  auto itere = Config.directories().end();
  bool hasMapping = Config.options().hasMappingFile();
  result = true;

  // Add INCLUDE Command
  auto includeCmd = eld::make<IncludeCmd>(filename, !state);
  includeCmd->setInputFileInContext(getContext());
  if (m_LeavingOutputSectDesc) {
    m_SectionsCmd->push_back(includeCmd);
    includeCmd->setParent(m_SectionsCmd);
  } else if (getParent()) {
    includeCmd->setParent(getParent());
    getParent()->push_back(includeCmd);
  } else
    m_CommandQueue.push_back(includeCmd);

  // If there is a mapping file, find the hash from the mapping file and
  // return a proper status.
  if (hasMapping) {
    std::string resolvedFilePath =
        m_Module.getConfig().getHashFromFile(filename);
    if (!llvm::sys::fs::exists(resolvedFilePath)) {
      if (state) {
        m_Module.setFailure(true);
        Config.raise(diag::fatal_cannot_read_input) << filename;
      }
      result = false;
    }
    return resolvedFilePath;
  }

  if (searchIncludeFile(filename, filename,
                        m_Module.getConfig().getDiagEngine())) {
    addInputToTar(filename, filename);
    return filename;
  }

  for (; iterb != itere; ++iterb) {
    std::string path = (*iterb)->name();
    path += "/";
    path += filename;
    if (searchIncludeFile(filename, path,
                          m_Module.getConfig().getDiagEngine())) {
      addInputToTar(filename, path);
      return path;
    }
  }
  result = false;
  if (!result && state) {
    Config.raise(diag::fatal_cannot_read_input) << filename;
    m_Module.setFailure(true);
    return filename;
  }
  return filename;
}

void ScriptFile::addOutputFormatCmd(const std::string &pName) {
  auto Cmd = make<OutputFormatCmd>(pName);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputFormatCmd(const std::string &pDefault,
                                    const std::string &pBig,
                                    const std::string &pLittle) {
  auto Cmd = make<OutputFormatCmd>(pDefault, pBig, pLittle);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addGroupCmd(StringList &pStringList,
                             const Attribute &attributes) {
  auto Cmd =
      make<GroupCmd>(m_Module.getConfig(), pStringList, attributes, *this);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addInputCmd(StringList &pStringList,
                             const Attribute &attributes) {
  auto Cmd =
      make<InputCmd>(m_Module.getConfig(), pStringList, attributes, *this);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputCmd(const std::string &pFileName) {
  auto Cmd = make<OutputCmd>(pFileName);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addSearchDirCmd(const std::string &pPath) {
  auto Cmd = make<SearchDirCmd>(pPath);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputArchCmd(const std::string &pArch) {
  auto Cmd = make<OutputArchCmd>(pArch);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_CommandQueue.push_back(Cmd);
}

void ScriptFile::addAssignment(const std::string &pSymbolName,
                               Expression *pExpr, Assignment::Type pType) {
  Assignment *newAssignment;
  if (m_bInSectionsCmd) {
    assert(!m_CommandQueue.empty());
    SectionsCmd *sections = m_SectionsCmd;
    if (m_bInOutputSectDesc) {
      assert(!sections->empty());
      newAssignment = make<Assignment>(Assignment::INPUT_SECTION, pType,
                                       pSymbolName, pExpr);
      newAssignment->setInputFileInContext(getContext());
      newAssignment->setParent(getParent());
      m_OutputSectDesc->push_back(newAssignment);
    } else {
      newAssignment = make<Assignment>(Assignment::OUTPUT_SECTION, pType,
                                       pSymbolName, pExpr);
      newAssignment->setInputFileInContext(getContext());
      newAssignment->setParent(getParent());
      sections->push_back(newAssignment);
    }
  } else {
    newAssignment = make<Assignment>(Assignment::OUTSIDE_SECTIONS, pType,
                                     pSymbolName, pExpr);
    newAssignment->setInputFileInContext(getContext());
    newAssignment->setParent(getParent());
    m_CommandQueue.push_back(newAssignment);
  }
}

bool ScriptFile::linkerScriptHasSectionsCommand() const {
  return m_bHasSectionsCmd;
}

void ScriptFile::enterSectionsCmd() {
  m_bHasSectionsCmd = true;
  m_bInSectionsCmd = true;
  auto Cmd = make<SectionsCmd>();
  Cmd->setInputFileInContext(getContext());
  m_CommandQueue.push_back(Cmd);
  m_SectionsCmd = Cmd;
  push(m_SectionsCmd);
  m_SectionsCmd->push_back(enterScope());
}

ScriptCommand *ScriptFile::enterScope() {
  auto Cmd = make<EnterScopeCmd>();
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  return Cmd;
}

ScriptCommand *ScriptFile::exitScope() {
  auto Cmd = make<ExitScopeCmd>();
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  pop();
  return Cmd;
}

void ScriptFile::leavingOutputSectDesc() { m_LeavingOutputSectDesc = true; }

void ScriptFile::leaveSectionsCmd() {
  m_SectionsCmd->push_back(exitScope());
  m_bInSectionsCmd = false;
}

void ScriptFile::enterOutputSectDesc(const std::string &pName,
                                     const OutputSectDesc::Prolog &pProlog) {
  assert(!m_CommandQueue.empty());
  assert(m_bInSectionsCmd);
  ASSERT(m_OutputSectDesc == nullptr, "OutputSectDesc should be null");
  m_OutputSectDesc = make<OutputSectDesc>(pName);
  m_OutputSectDesc->setParent(getParent());
  m_OutputSectDesc->setInputFileInContext(getContext());
  m_OutputSectDesc->setProlog(pProlog);
  m_SectionsCmd->push_back(m_OutputSectDesc);
  m_bInOutputSectDesc = true;
  m_firstLinkerScriptWithOutputSection = true;
  push(m_OutputSectDesc);
  m_OutputSectDesc->push_back(enterScope());
  m_LeavingOutputSectDesc = false;
  m_Module.addToOutputSectionDescNameSet(pName);
}

void ScriptFile::leaveOutputSectDesc(const OutputSectDesc::Epilog &pEpilog) {
  if (ScriptFileStack.empty())
    return;
  assert(!m_CommandQueue.empty() && m_bInSectionsCmd);
  bool outhasPhdrs = pEpilog.hasPhdrs();
  hasEpilogPhdrs |= outhasPhdrs;

  assert(!m_SectionsCmd->empty() && m_bInOutputSectDesc);
  auto E = m_OutputSectDesc->setEpilog(pEpilog);
  if (!E)
    m_Module.getConfig().raiseDiagEntry(std::move(E.error()));

  m_LeavingOutputSectDesc = true;

  // Add a default spec to catch rules that belong to the output section.
  InputSectDesc::Spec defaultSpec;
  defaultSpec.initialize();
  StringList *stringList = createStringList();
  defaultSpec.m_pWildcardFile = createWildCardPattern(createParserStr("*", 1));
  stringList->push_back(
      createWildCardPattern(eld::make<StrToken>(m_OutputSectDesc->name())));
  defaultSpec.m_pWildcardSections = stringList;
  defaultSpec.m_pArchiveMember = nullptr;
  defaultSpec.m_pIsArchive = 0;
  InputSectDesc *DefaultSpec = make<InputSectDesc>(
      m_Module.getScript().getIncrementedRuleCount(),
      InputSectDesc::SpecialNoKeep, defaultSpec, *m_OutputSectDesc);
  DefaultSpec->setInputFileInContext(
      m_Module.getInternalInput(Module::InternalInputType::Script));
  DefaultSpec->setParent(getParent());
  m_OutputSectDesc->push_back(DefaultSpec);

  m_OutputSectDesc->push_back(exitScope());

  m_bInOutputSectDesc = false;

  if (outhasPhdrs)
    oldEpilogPhdrs = pEpilog.phdrs();

  // If no PHDR specified and output has NOLOAD, we need to consider this
  // separately.
  if (m_OutputSectDesc->prolog().type() == OutputSectDesc::NOLOAD) {
    m_OutputSectDesc = nullptr;
    return;
  }

  if (hasEpilogPhdrs && !outhasPhdrs && oldEpilogPhdrs) {
    if (oldEpilogPhdrs->size() == 1)
      m_OutputSectDesc->epilog().m_pPhdrs = oldEpilogPhdrs;
    else if (oldEpilogPhdrs->size() > 1) {
      m_OutputSectDesc->epilog().m_pPhdrs = createStringList();
      m_OutputSectDesc->epilog().m_pPhdrs->push_back(
          createStrToken(oldEpilogPhdrs->back()->name()));
    } else
      m_Module.getConfig().raise(diag::err_cant_figure_which_phdr)
          << m_OutputSectDesc->name();
  }
  m_OutputSectDesc = nullptr;
}

void ScriptFile::addInputSectDesc(InputSectDesc::Policy pPolicy,
                                  const InputSectDesc::Spec &pSpec) {
  assert(!m_CommandQueue.empty());
  assert(m_bInSectionsCmd);

  LayoutPrinter *printer = m_Module.getLayoutPrinter();

  assert(!m_SectionsCmd->empty() && m_bInOutputSectDesc);

  if (printer)
    printer->recordLinkerScriptRule();

  InputSectDesc *Desc = nullptr;

  if (!pSpec.m_pWildcardSections) {
    m_Module.getConfig().raise(diag::files_no_wildcard_rules)
        << pSpec.file().name() << m_OutputSectDesc->name();
    InputSectDesc::Spec NoWildcardSectionsSpec = pSpec;
    StringList *stringList = createStringList();
    // Add a rule to grab all the sections from the input file
    // This way no rule matching logic needs to be modified
    stringList->push_back(createWildCardPattern(eld::make<StrToken>("*")));
    NoWildcardSectionsSpec.m_pWildcardSections = stringList;
    Desc =
        make<InputSectDesc>(m_Module.getScript().getIncrementedRuleCount(),
                            pPolicy, NoWildcardSectionsSpec, *m_OutputSectDesc);
  } else {
    Desc = make<InputSectDesc>(m_Module.getScript().getIncrementedRuleCount(),
                               pPolicy, pSpec, *m_OutputSectDesc);
  }
  Desc->setInputFileInContext(getContext());
  Desc->setParent(getParent());
  m_OutputSectDesc->push_back(Desc);
}

StringList *ScriptFile::createStringList() {
  return (m_pStringList = make<StringList>());
}

ExcludeFiles *ScriptFile::createExcludeFiles() {
  return (m_pExcludeFiles = make<ExcludeFiles>(ExcludeFiles()));
}

ExcludePattern *ScriptFile::createExcludePattern(StrToken *S) {
  std::string name = S->name();
  size_t colonPos = name.find(":");
  WildcardPattern *archivePattern = nullptr;
  WildcardPattern *filePattern = nullptr;
  StrToken *archiveToken = nullptr;
  StrToken *fileToken = nullptr;
  // Handles: <file>
  if (colonPos == std::string::npos) {
    fileToken = createStrToken(name);
    filePattern = createWildCardPattern(fileToken);
  } else {
    // Handles: <archive>:
    archiveToken = createStrToken(name.substr(0, colonPos));
    archivePattern = createWildCardPattern(archiveToken);
    // Handles: <archive>:<member>
    if (colonPos != name.size() - 1) {
      fileToken = createStrToken(name.substr(colonPos + 1));
      filePattern = createWildCardPattern(fileToken);
    }
  }
  return make<ExcludePattern>(archivePattern, filePattern);
}

void ScriptFile::setAsNeeded(bool pEnable) { m_bAsNeeded = pEnable; }

StrToken *ScriptFile::createStrToken(const std::string &pString) {
  return make<StrToken>(pString);
}

FileToken *ScriptFile::createFileToken(const std::string &pString,
                                       bool AsNeeded) {
  return make<FileToken>(pString, AsNeeded);
}

NameSpec *ScriptFile::createNameSpecToken(const std::string &pString,
                                          bool AsNeeded) {
  return make<NameSpec>(pString, AsNeeded);
}

WildcardPattern *
ScriptFile::createWildCardPattern(StrToken *S, WildcardPattern::SortPolicy P,
                                  ExcludeFiles *E) {
  auto F = m_WildcardPatternMap.find(S->name());
  if (F != m_WildcardPatternMap.end())
    return F->second;
  WildcardPattern *Pat = make<WildcardPattern>(S, P, E);
  m_Module.getScript().registerWildCardPattern(Pat);
  return Pat;
}

WildcardPattern *ScriptFile::createWildCardPattern(
    llvm::StringRef S, WildcardPattern::SortPolicy P, ExcludeFiles *E) {
  StrToken *tok = createStrToken(S.str());
  return createWildCardPattern(tok, P, E);
}

ScriptSymbol *ScriptFile::createScriptSymbol(const StrToken *S) const {
  return make<ScriptSymbol>(S->name());
}

ScriptSymbol *ScriptFile::createScriptSymbol(llvm::StringRef S) const {
  return make<ScriptSymbol>(S.str());
}

StrToken *ScriptFile::createParserStr(const char *pText, size_t pLength) {
  std::string text = std::string(pText, pLength);
  // Remove double-quote characters.
  text.erase(std::remove(text.begin(), text.end(), '"'), text.end());
  return make<eld::StrToken>(text);
}

StrToken *ScriptFile::createParserStr(llvm::StringRef s) {
  bool isQuoted = false;
  if (s.starts_with("\"")) {
    s = s.substr(1, s.size() - 2);
    isQuoted = true;
  }
  StrToken *tok = make<eld::StrToken>(s.str());
  if (isQuoted)
    tok->setQuoted();
  return tok;
}

void ScriptFile::enterPhdrsCmd() {
  m_bInPhdrsCmd = true;
  m_Module.getScript().setPhdrsSpecified();
  auto Cmd = make<PhdrsCmd>();
  Cmd->setInputFileInContext(getContext());
  m_CommandQueue.push_back(Cmd);
  m_PhdrsCmd = Cmd;
  push(m_PhdrsCmd);
  m_PhdrsCmd->push_back(enterScope());
}

void ScriptFile::leavePhdrsCmd() {
  m_bInPhdrsCmd = false;
  m_PhdrsCmd->push_back(exitScope());
}

void ScriptFile::addPhdrDesc(const PhdrSpec &pSpec) {
  assert(!m_CommandQueue.empty());
  assert(m_bInPhdrsCmd);
  auto Cmd = make<PhdrDesc>(pSpec);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  m_PhdrsCmd->push_back(Cmd);
}

PluginCmd *ScriptFile::addPlugin(plugin::Plugin::Type T, std::string Name,
                                 std::string R, std::string O) {
  auto Plugin = make<PluginCmd>(T, Name, R, O);
  Plugin->setParent(getParent());
  Plugin->setInputFileInContext(getContext());
  m_CommandQueue.push_back(Plugin);
  return Plugin;
}

InputFile *ScriptFile::getContext() const {
  return ScriptFileStack.empty() ? nullptr : ScriptFileStack.top();
}

void ScriptFile::setContext(InputFile *file) {
  // FIXME: Ideally this should never be hit, maybe add an assert?
  if (getContext() == file)
    return;
  ScriptFileStack.push(file);
}

llvm::StringRef ScriptFile::getPath() const {
  return m_Module.saveString(m_LinkerScriptFile.getInput()->decoratedPath());
}

std::vector<ScriptSymbol *> *ScriptFile::createDynamicList() {
  if (DynamicListSymbols)
    return DynamicListSymbols;
  DynamicListSymbols = make<std::vector<ScriptSymbol *>>();
  return DynamicListSymbols;
}

void ScriptFile::addSymbolToDynamicList(ScriptSymbol *S) {
  if (m_Module.getPrinter()->isVerbose())
    m_Module.getConfig().raise(diag::reading_dynamic_list)
        << getContext()->getInput()->decoratedPath() << S->name();
  DynamicListSymbols->push_back(S);
}

void ScriptFile::addSymbolToExternList(StrToken *S) {
  if (m_Module.getPrinter()->isVerbose())
    m_Module.getConfig().raise(diag::reading_extern_list)
        << getContext()->getInput()->decoratedPath() << S->name();
  m_ExternCmd->addExternCommand(S);
}

ExternCmd *ScriptFile::createExternCmd() {
  if (!m_ExternCmd)
    m_ExternCmd = addExtern(*createStringList());
  return m_ExternCmd;
}

VersionScript *ScriptFile::createVersionScript() {
  if (!m_VersionScript)
    m_VersionScript = make<eld::VersionScript>(&m_LinkerScriptFile);
  return m_VersionScript;
}

VersionScript *ScriptFile::getVersionScript() { return m_VersionScript; }

void ScriptFile::addMemoryRegion(StrToken *Name, StrToken *Attributes,
                                 Expression *Origin, Expression *Length) {
  if (!m_MemoryCmd) {
    m_MemoryCmd = eld::make<MemoryCmd>();
    m_MemoryCmd->setInputFileInContext(getContext());
    m_MemoryCmd->setParent(getParent());
    m_CommandQueue.push_back(m_MemoryCmd);
  }
  MemoryDesc *Desc =
      eld::make<MemoryDesc>(MemorySpec(Name, Attributes, Origin, Length));
  Desc->setInputFileInContext(getContext());
  Desc->setParent(getParent());
  m_MemoryCmd->push_back(Desc);
}

void ScriptFile::addOutputSectData(OutputSectData::OSDKind dataKind,
                                   Expression *expr) {
  assert(m_bInSectionsCmd);

  LayoutPrinter *printer = m_Module.getLayoutPrinter();
  if (printer)
    printer->recordLinkerScriptRule();

  ASSERT(expr, "expr must not be null!");

  OutputSectData *OSD =
      OutputSectData::Create(m_Module.getScript().getIncrementedRuleCount(),
                             *m_OutputSectDesc, dataKind, *expr);
  OSD->setInputFileInContext(getContext());
  OSD->setParent(getParent());
  m_OutputSectDesc->push_back(OSD);
}

void ScriptFile::addRegionAlias(const StrToken *alias, const StrToken *region) {
  RegionAlias *R = eld::make<RegionAlias>(alias, region);
  R->setInputFileInContext(getContext());
  R->setParent(getParent());
  m_CommandQueue.push_back(R);
}
