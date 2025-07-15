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

static eld::StringList *OldEpilogPhdrs = nullptr;
static bool HasEpilogPhdrs = false;
bool ScriptFile::IsFirstLinkerScriptWithSectionCommand = false;

//===----------------------------------------------------------------------===//
// ScriptFile
//===----------------------------------------------------------------------===//
ScriptFile::ScriptFile(Kind PKind, Module &CurModule, LinkerScriptFile &PInput,
                       InputBuilder &PBuilder, GNULDBackend &PBackend)
    : ScriptFileKind(PKind), ThisModule(CurModule),
      ThisLinkerScriptFile(PInput), ThisBackend(PBackend),
      Name(PInput.getInput()->getResolvedPath().native()),
      LinkerScriptHasSectionsCommand(false),
      ScriptStateInSectionsCommmand(false),
      ScriptStateInsideOutputSection(false), ScriptFileStringList(nullptr),
      LinkerScriptSectionsCommand(nullptr), LinkerScriptPHDRSCommand(nullptr),
      OutputSectionDescription(nullptr), LinkerScriptHasAsNeeded(false) {
  setContext(&PInput);
}

ScriptFile::~ScriptFile() {}

void ScriptFile::dump(llvm::raw_ostream &Outs) const {
  for (const auto &Elem : *this)
    (Elem)->dump(Outs);
}

eld::Expected<void> ScriptFile::activate(Module &CurModule) {
  for (auto &SC : *this) {
    eld::Expected<void> E = SC->activate(CurModule);
    if (!E)
      return E;
    // There can be multiple scripts included and the linker needs to be parse
    // each one of them.
    CurModule.getScript().addScriptCommand(SC);
  }
  if (ScriptFileKind == Kind::DynamicList && DynamicListSymbols) {
    for (ScriptSymbol *Sym : *DynamicListSymbols)
      ELDEXP_RETURN_DIAGENTRY_IF_ERROR(Sym->activate());
  }
  return eld::Expected<void>();
}

ScriptCommand *ScriptFile::addEntryPoint(const std::string &Symbol) {
  auto *Entry = make<EntryCmd>(Symbol);
  Entry->setInputFileInContext(getContext());
  Entry->setParent(getParent());

  if (ScriptStateInSectionsCommmand) {
    LinkerScriptSectionsCommand->pushBack(Entry);
  } else {
    LinkerScriptCommandQueue.push_back(Entry);
  }
  return Entry;
}

ExternCmd *ScriptFile::addExtern(StringList &List) {
  auto *ExternCmd = make<eld::ExternCmd>(List);
  ExternCmd->setInputFileInContext(getContext());
  ExternCmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(ExternCmd);
  return ExternCmd;
}

void ScriptFile::addNoCrossRefs(StringList &List) {
  auto *NoCrossRefs =
      make<NoCrossRefsCmd>(List, LinkerScriptCommandQueue.size());
  NoCrossRefs->setInputFileInContext(getContext());
  NoCrossRefs->setParent(getParent());
  LinkerScriptCommandQueue.push_back(NoCrossRefs);
}

void ScriptFile::addInputToTar(const std::string &Filename,
                               const std::string &ResolvedPath) const {
  if (!ThisModule.getOutputTarWriter())
    return;
  ThisModule.getOutputTarWriter()->createAndAddScriptFile(Filename,
                                                          ResolvedPath);
}

namespace {

inline bool searchIncludeFile(llvm::StringRef Name, llvm::StringRef FileName,
                              DiagnosticEngine *DiagEngine) {
  bool Found = llvm::sys::fs::exists(FileName);
  DiagEngine->raise(Diag::verbose_trying_script_include_file)
      << FileName << Name << (Found ? "found" : "not found");
  return Found;
}

} // namespace

InputToken* ScriptFile::findResolvedFilename(InputToken *input) {
  LinkerConfig &Config = ThisModule.getConfig();
  if (Config.options().hasMappingFile()) {
    std::string ResolvedFilename =
        ThisModule.getConfig().getHashFromFile(input->name());
    if (llvm::sys::fs::exists(ResolvedFilename)) {
      if(llvm::isa<eld::NameSpec>(input))
      return createNameSpecToken(ResolvedFilename, input->asNeeded());
    else
      return createFileToken(ResolvedFilename, input->asNeeded());
    }
  }
  return input;
}

std::string ScriptFile::findIncludeFile(const std::string &Filename,
                                        bool &Result, bool State) {
  LinkerConfig &Config = ThisModule.getConfig();
  auto Iterb = Config.directories().begin();
  auto Itere = Config.directories().end();
  bool HasMapping = Config.options().hasMappingFile();
  Result = true;

  // Add INCLUDE Command
  auto *IncludeCmd = eld::make<eld::IncludeCmd>(Filename, !State);
  IncludeCmd->setInputFileInContext(getContext());
  if (IsLeavingOutputSectDesc) {
    LinkerScriptSectionsCommand->pushBack(IncludeCmd);
    IncludeCmd->setParent(LinkerScriptSectionsCommand);
  } else if (getParent()) {
    IncludeCmd->setParent(getParent());
    getParent()->pushBack(IncludeCmd);
  } else
    LinkerScriptCommandQueue.push_back(IncludeCmd);

  // If there is a mapping file, find the hash from the mapping file and
  // return a proper status.
  if (HasMapping) {
    std::string ResolvedFilePath =
        ThisModule.getConfig().getHashFromFile(Filename);
    if (!llvm::sys::fs::exists(ResolvedFilePath)) {
      if (State) {
        ThisModule.setFailure(true);
        Config.raise(Diag::fatal_cannot_read_input) << Filename;
      }
      Result = false;
    }
    return ResolvedFilePath;
  }

  if (searchIncludeFile(Filename, Filename,
                        ThisModule.getConfig().getDiagEngine())) {
    addInputToTar(Filename, Filename);
    return Filename;
  }

  for (; Iterb != Itere; ++Iterb) {
    std::string Path = (*Iterb)->name();
    Path += "/";
    Path += Filename;
    if (searchIncludeFile(Filename, Path,
                          ThisModule.getConfig().getDiagEngine())) {
      addInputToTar(Filename, Path);
      return Path;
    }
  }
  Result = false;
  if (!Result && State) {
    Config.raise(Diag::fatal_cannot_read_input) << Filename;
    ThisModule.setFailure(true);
    return Filename;
  }
  return Filename;
}

void ScriptFile::addOutputFormatCmd(const std::string &PName) {
  auto *Cmd = make<OutputFormatCmd>(PName);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputFormatCmd(const std::string &PDefault,
                                    const std::string &PBig,
                                    const std::string &PLittle) {
  auto *Cmd = make<OutputFormatCmd>(PDefault, PBig, PLittle);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addGroupCmd(StringList &PStringList,
                             const Attribute &Attributes) {
  auto *Cmd =
      make<GroupCmd>(ThisModule.getConfig(), PStringList, Attributes, *this);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addInputCmd(StringList &PStringList,
                             const Attribute &Attributes) {
  auto *Cmd =
      make<InputCmd>(ThisModule.getConfig(), PStringList, Attributes, *this);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputCmd(const std::string &PFileName) {
  auto *Cmd = make<OutputCmd>(PFileName);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addSearchDirCmd(const std::string &PPath) {
  auto *Cmd = make<SearchDirCmd>(PPath);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addOutputArchCmd(const std::string &PArch) {
  auto *Cmd = make<OutputArchCmd>(PArch);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptCommandQueue.push_back(Cmd);
}

void ScriptFile::addAssignment(const std::string &SymbolName,
                               Expression *ScriptExpression,
                               Assignment::Type AssignmentType) {
  Assignment *NewAssignment = nullptr;
  if (ScriptStateInSectionsCommmand) {
    assert(!LinkerScriptCommandQueue.empty());
    SectionsCmd *Sections = LinkerScriptSectionsCommand;
    if (ScriptStateInsideOutputSection) {
      assert(!Sections->empty());
      NewAssignment =
          make<Assignment>(Assignment::INPUT_SECTION, AssignmentType,
                           SymbolName, ScriptExpression);
      NewAssignment->setInputFileInContext(getContext());
      NewAssignment->setParent(getParent());
      OutputSectionDescription->pushBack(NewAssignment);
    } else {
      NewAssignment =
          make<Assignment>(Assignment::OUTPUT_SECTION, AssignmentType,
                           SymbolName, ScriptExpression);
      NewAssignment->setInputFileInContext(getContext());
      NewAssignment->setParent(getParent());
      Sections->pushBack(NewAssignment);
    }
  } else {
    NewAssignment =
        make<Assignment>(Assignment::OUTSIDE_SECTIONS, AssignmentType,
                         SymbolName, ScriptExpression);
    NewAssignment->setInputFileInContext(getContext());
    NewAssignment->setParent(getParent());
    LinkerScriptCommandQueue.push_back(NewAssignment);
  }
  Assignments.push_back(NewAssignment);
}

bool ScriptFile::linkerScriptHasSectionsCommand() const {
  return LinkerScriptHasSectionsCommand;
}

void ScriptFile::enterSectionsCmd() {
  LinkerScriptHasSectionsCommand = true;
  ScriptStateInSectionsCommmand = true;
  auto *Cmd = make<SectionsCmd>();
  Cmd->setInputFileInContext(getContext());
  LinkerScriptCommandQueue.push_back(Cmd);
  LinkerScriptSectionsCommand = Cmd;
  push(LinkerScriptSectionsCommand);
  LinkerScriptSectionsCommand->pushBack(enterScope());
}

ScriptCommand *ScriptFile::enterScope() {
  auto *Cmd = make<EnterScopeCmd>();
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  return Cmd;
}

ScriptCommand *ScriptFile::exitScope() {
  auto *Cmd = make<ExitScopeCmd>();
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  pop();
  return Cmd;
}

void ScriptFile::leavingOutputSectDesc() { IsLeavingOutputSectDesc = true; }

void ScriptFile::leaveSectionsCmd() {
  LinkerScriptSectionsCommand->pushBack(exitScope());
  ScriptStateInSectionsCommmand = false;
}

void ScriptFile::enterOutputSectDesc(const std::string &PName,
                                     const OutputSectDesc::Prolog &PProlog) {
  assert(!LinkerScriptCommandQueue.empty());
  assert(ScriptStateInSectionsCommmand);
  ASSERT(OutputSectionDescription == nullptr, "OutputSectDesc should be null");
  OutputSectionDescription = make<OutputSectDesc>(PName);
  OutputSectionDescription->setParent(getParent());
  OutputSectionDescription->setInputFileInContext(getContext());
  OutputSectionDescription->setProlog(PProlog);
  LinkerScriptSectionsCommand->pushBack(OutputSectionDescription);
  ScriptStateInsideOutputSection = true;
  IsFirstLinkerScriptWithSectionCommand = true;
  push(OutputSectionDescription);
  OutputSectionDescription->pushBack(enterScope());
  IsLeavingOutputSectDesc = false;
  ThisModule.addToOutputSectionDescNameSet(PName);
}

void ScriptFile::leaveOutputSectDesc(const OutputSectDesc::Epilog &PEpilog) {
  if (ScriptFileStack.empty())
    return;
  assert(!LinkerScriptCommandQueue.empty() && ScriptStateInSectionsCommmand);
  bool OuthasPhdrs = PEpilog.hasPhdrs();
  HasEpilogPhdrs |= OuthasPhdrs;

  assert(!LinkerScriptSectionsCommand->empty() &&
         ScriptStateInsideOutputSection);
  auto E = OutputSectionDescription->setEpilog(PEpilog);
  if (!E)
    ThisModule.getConfig().raiseDiagEntry(std::move(E.error()));

  IsLeavingOutputSectDesc = true;

  // Add a default spec to catch rules that belong to the output section.
  InputSectDesc::Spec DefaultSpec;
  DefaultSpec.initialize();
  StringList *StringList = createStringList();
  DefaultSpec.WildcardFilePattern =
      createWildCardPattern(createParserStr("*", 1));
  StringList->pushBack(createWildCardPattern(
      eld::make<StrToken>(OutputSectionDescription->name())));
  DefaultSpec.WildcardSectionPattern = StringList;
  DefaultSpec.InputArchiveMember = nullptr;
  DefaultSpec.InputIsArchive = 0;
  InputSectDesc *Spec = make<InputSectDesc>(
      ThisModule.getScript().getIncrementedRuleCount(),
      InputSectDesc::SpecialNoKeep, DefaultSpec, *OutputSectionDescription);
  Spec->setInputFileInContext(
      ThisModule.getInternalInput(Module::InternalInputType::Script));
  Spec->setParent(getParent());
  OutputSectionDescription->pushBack(Spec);

  OutputSectionDescription->pushBack(exitScope());

  ScriptStateInsideOutputSection = false;

  if (OuthasPhdrs)
    OldEpilogPhdrs = PEpilog.phdrs();

  // If no PHDR specified and output has NOLOAD, we need to consider this
  // separately.
  if (OutputSectionDescription->prolog().type() == OutputSectDesc::NOLOAD) {
    OutputSectionDescription = nullptr;
    return;
  }

  if (HasEpilogPhdrs && !OuthasPhdrs && OldEpilogPhdrs) {
    if (OldEpilogPhdrs->size() == 1)
      OutputSectionDescription->epilog().ScriptPhdrs = OldEpilogPhdrs;
    else if (OldEpilogPhdrs->size() > 1) {
      OutputSectionDescription->epilog().ScriptPhdrs = createStringList();
      OutputSectionDescription->epilog().ScriptPhdrs->pushBack(
          createStrToken(OldEpilogPhdrs->back()->name()));
    } else
      ThisModule.getConfig().raise(Diag::err_cant_figure_which_phdr)
          << OutputSectionDescription->name();
  }
  OutputSectionDescription = nullptr;
}

void ScriptFile::addInputSectDesc(InputSectDesc::Policy PPolicy,
                                  const InputSectDesc::Spec &PSpec) {
  assert(!LinkerScriptCommandQueue.empty());
  assert(ScriptStateInSectionsCommmand);

  LayoutInfo *layoutInfo = ThisModule.getLayoutInfo();

  assert(!LinkerScriptSectionsCommand->empty() &&
         ScriptStateInsideOutputSection);

  if (layoutInfo)
    layoutInfo->recordLinkerScriptRule();

  InputSectDesc *Desc = nullptr;

  if (!PSpec.WildcardSectionPattern) {
    ThisModule.getConfig().raise(Diag::files_no_wildcard_rules)
        << PSpec.file().name() << OutputSectionDescription->name();
    InputSectDesc::Spec NoWildcardSectionsSpec = PSpec;
    StringList *StringList = createStringList();
    // Add a rule to grab all the sections from the input file
    // This way no rule matching logic needs to be modified
    StringList->pushBack(createWildCardPattern(eld::make<StrToken>("*")));
    NoWildcardSectionsSpec.WildcardSectionPattern = StringList;
    Desc = make<InputSectDesc>(ThisModule.getScript().getIncrementedRuleCount(),
                               PPolicy, NoWildcardSectionsSpec,
                               *OutputSectionDescription);
  } else {
    Desc = make<InputSectDesc>(ThisModule.getScript().getIncrementedRuleCount(),
                               PPolicy, PSpec, *OutputSectionDescription);
  }
  Desc->setInputFileInContext(getContext());
  Desc->setParent(getParent());
  OutputSectionDescription->pushBack(Desc);
}

StringList *ScriptFile::createStringList() {
  return (ScriptFileStringList = make<StringList>());
}

ExcludeFiles *ScriptFile::createExcludeFiles() {
  return (MPExcludeFiles = make<ExcludeFiles>(ExcludeFiles()));
}

ExcludePattern *ScriptFile::createExcludePattern(StrToken *S) {
  std::string Name = S->name();
  size_t ColonPos = Name.find(":");
  WildcardPattern *ArchivePattern = nullptr;
  WildcardPattern *FilePattern = nullptr;
  StrToken *ArchiveToken = nullptr;
  StrToken *FileToken = nullptr;
  // Handles: <file>
  if (ColonPos == std::string::npos) {
    FileToken = createStrToken(Name);
    FilePattern = createWildCardPattern(FileToken);
  } else {
    // Handles: <archive>:
    ArchiveToken = createStrToken(Name.substr(0, ColonPos));
    ArchivePattern = createWildCardPattern(ArchiveToken);
    // Handles: <archive>:<member>
    if (ColonPos != Name.size() - 1) {
      FileToken = createStrToken(Name.substr(ColonPos + 1));
      FilePattern = createWildCardPattern(FileToken);
    }
  }
  return make<ExcludePattern>(ArchivePattern, FilePattern);
}

void ScriptFile::setAsNeeded(bool PEnable) {
  LinkerScriptHasAsNeeded = PEnable;
}

StrToken *ScriptFile::createStrToken(const std::string &PString) {
  return make<StrToken>(PString);
}

FileToken *ScriptFile::createFileToken(const std::string &PString,
                                       bool AsNeeded) {
  return make<FileToken>(PString, AsNeeded);
}

NameSpec *ScriptFile::createNameSpecToken(const std::string &PString,
                                          bool AsNeeded) {
  return make<NameSpec>(PString, AsNeeded);
}

WildcardPattern *
ScriptFile::createWildCardPattern(StrToken *S, WildcardPattern::SortPolicy P,
                                  ExcludeFiles *E) {
  auto F = ScriptWildcardPatternMap.find(S->name());
  if (F != ScriptWildcardPatternMap.end())
    return F->second;
  WildcardPattern *Pat = make<WildcardPattern>(S, P, E);
  ThisModule.getScript().registerWildCardPattern(Pat);
  return Pat;
}

WildcardPattern *ScriptFile::createWildCardPattern(
    llvm::StringRef S, WildcardPattern::SortPolicy P, ExcludeFiles *E) {
  StrToken *Tok = createStrToken(S.str());
  return createWildCardPattern(Tok, P, E);
}

ScriptSymbol *ScriptFile::createScriptSymbol(const StrToken *S) const {
  return make<ScriptSymbol>(S->name());
}

ScriptSymbol *ScriptFile::createScriptSymbol(llvm::StringRef S) const {
  return make<ScriptSymbol>(S.str());
}

StrToken *ScriptFile::createParserStr(const char *PText, size_t PLength) {
  std::string Text = std::string(PText, PLength);
  // Remove double-quote characters.
  Text.erase(std::remove(Text.begin(), Text.end(), '"'), Text.end());
  return make<eld::StrToken>(Text);
}

StrToken *ScriptFile::createParserStr(llvm::StringRef S) {
  bool IsQuoted = false;
  if (S.starts_with("\"")) {
    S = S.substr(1, S.size() - 2);
    IsQuoted = true;
  }
  StrToken *Tok = make<eld::StrToken>(S.str());
  if (IsQuoted)
    Tok->setQuoted();
  return Tok;
}

void ScriptFile::enterPhdrsCmd() {
  ScriptStateInPHDRSCommand = true;
  ThisModule.getScript().setPhdrsSpecified();
  auto *Cmd = make<PhdrsCmd>();
  Cmd->setInputFileInContext(getContext());
  LinkerScriptCommandQueue.push_back(Cmd);
  LinkerScriptPHDRSCommand = Cmd;
  push(LinkerScriptPHDRSCommand);
  LinkerScriptPHDRSCommand->pushBack(enterScope());
}

void ScriptFile::leavePhdrsCmd() {
  ScriptStateInPHDRSCommand = false;
  LinkerScriptPHDRSCommand->pushBack(exitScope());
}

void ScriptFile::addPhdrDesc(const PhdrSpec &PSpec) {
  assert(!LinkerScriptCommandQueue.empty());
  assert(ScriptStateInPHDRSCommand);
  auto *Cmd = make<PhdrDesc>(PSpec);
  Cmd->setInputFileInContext(getContext());
  Cmd->setParent(getParent());
  LinkerScriptPHDRSCommand->pushBack(Cmd);
}

PluginCmd *ScriptFile::addPlugin(plugin::Plugin::Type T, std::string Name,
                                 std::string R, std::string O) {
  auto *Plugin = make<PluginCmd>(T, Name, R, O);
  Plugin->setParent(getParent());
  Plugin->setInputFileInContext(getContext());
  LinkerScriptCommandQueue.push_back(Plugin);
  return Plugin;
}

InputFile *ScriptFile::getContext() const {
  return ScriptFileStack.empty() ? nullptr : ScriptFileStack.top();
}

void ScriptFile::setContext(InputFile *File) {
  // FIXME: Ideally this should never be hit, maybe add an assert?
  if (getContext() == File)
    return;
  ScriptFileStack.push(File);
}

llvm::StringRef ScriptFile::getPath() const {
  return ThisModule.saveString(
      ThisLinkerScriptFile.getInput()->decoratedPath());
}

std::vector<ScriptSymbol *> *ScriptFile::createDynamicList() {
  if (DynamicListSymbols)
    return DynamicListSymbols;
  DynamicListSymbols = make<std::vector<ScriptSymbol *>>();
  return DynamicListSymbols;
}

void ScriptFile::addSymbolToDynamicList(ScriptSymbol *S) {
  if (ThisModule.getPrinter()->isVerbose())
    ThisModule.getConfig().raise(Diag::reading_dynamic_list)
        << getContext()->getInput()->decoratedPath() << S->name();
  DynamicListSymbols->push_back(S);
}

void ScriptFile::addSymbolToExternList(StrToken *S) {
  if (ThisModule.getPrinter()->isVerbose())
    ThisModule.getConfig().raise(Diag::reading_extern_list)
        << getContext()->getInput()->decoratedPath() << S->name();
  ScriptFileExternCommand->addExternCommand(S);
}

ExternCmd *ScriptFile::createExternCmd() {
  if (!ScriptFileExternCommand)
    ScriptFileExternCommand = addExtern(*createStringList());
  return ScriptFileExternCommand;
}

VersionScript *ScriptFile::createVersionScript() {
  if (!LinkerVersionScript)
    LinkerVersionScript = make<eld::VersionScript>(&ThisLinkerScriptFile);
  return LinkerVersionScript;
}

VersionScript *ScriptFile::getVersionScript() { return LinkerVersionScript; }

void ScriptFile::addMemoryRegion(StrToken *Name, StrToken *Attributes,
                                 Expression *Origin, Expression *Length) {
  if (!MemoryCmd) {
    MemoryCmd = eld::make<eld::MemoryCmd>();
    MemoryCmd->setInputFileInContext(getContext());
    MemoryCmd->setParent(getParent());
    LinkerScriptCommandQueue.push_back(MemoryCmd);
  }
  MemoryDesc *Desc =
      eld::make<MemoryDesc>(MemorySpec(Name, Attributes, Origin, Length));
  Desc->setInputFileInContext(getContext());
  Desc->setParent(getParent());
  MemoryCmd->pushBack(Desc);
}

void ScriptFile::addOutputSectData(OutputSectData::OSDKind DataKind,
                                   Expression *Expr) {
  assert(ScriptStateInSectionsCommmand);

  LayoutInfo *layoutInfo = ThisModule.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordLinkerScriptRule();

  ASSERT(Expr, "expr must not be null!");

  OutputSectData *OSD =
      OutputSectData::create(ThisModule.getScript().getIncrementedRuleCount(),
                             *OutputSectionDescription, DataKind, *Expr);
  OSD->setInputFileInContext(getContext());
  OSD->setParent(getParent());
  OutputSectionDescription->pushBack(OSD);
}

void ScriptFile::addRegionAlias(const StrToken *Alias, const StrToken *Region) {
  RegionAlias *R = eld::make<RegionAlias>(Alias, Region);
  R->setInputFileInContext(getContext());
  R->setParent(getParent());
  LinkerScriptCommandQueue.push_back(R);
}

void ScriptFile::processAssignments() {
  for (auto &Assign : getAssignments()) {
    Assign->processAssignment(ThisModule, ThisLinkerScriptFile);
  }
}
