//===- LinkerScript.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Core/LinkerScript.h"
#include "eld/PluginAPI/LinkerScript.h"
#include "eld/Script/EnterScopeCmd.h"
#include "eld/Script/EntryCmd.h"
#include "eld/Script/ExitScopeCmd.h"
#include "eld/Script/ExternCmd.h"
#include "eld/Script/GroupCmd.h"
#include "eld/Script/IncludeCmd.h"
#include "eld/Script/InputCmd.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/NoCrossRefsCmd.h"
#include "eld/Script/OutputArchCmd.h"
#include "eld/Script/OutputCmd.h"
#include "eld/Script/OutputFormatCmd.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/OutputSectDesc.h"
#include "eld/Script/PhdrDesc.h"
#include "eld/Script/PhdrsCmd.h"
#include "eld/Script/PluginCmd.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/SearchDirCmd.h"
#include "eld/Script/SectionsCmd.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/Support/Utils.h"
#include "eld/Target/ELFSegment.h"
#include <chrono>
#include <sstream>

using namespace eld;
using namespace eld::plugin;

// ScriptCommand
plugin::Script::ScriptCommand::CommandKind
plugin::Script::ScriptCommand::getCommandKind(eld::ScriptCommand *SC) {
  switch (SC->getKind()) {
  case eld::ScriptCommand::PHDRS:
    return plugin::Script::ScriptCommand::PHDRS;
  case eld::ScriptCommand::PHDR_DESC:
    return plugin::Script::ScriptCommand::PHDRDesc;
  case eld::ScriptCommand::ASSIGNMENT:
    return plugin::Script::ScriptCommand::Assignment;
  case eld::ScriptCommand::ENTER_SCOPE:
    return plugin::Script::ScriptCommand::EnterScope;
  case eld::ScriptCommand::ENTRY:
    return plugin::Script::ScriptCommand::Entry;
  case eld::ScriptCommand::EXIT_SCOPE:
    return plugin::Script::ScriptCommand::ExitScope;
  case eld::ScriptCommand::EXTERN:
    return plugin::Script::ScriptCommand::Extern;
  case eld::ScriptCommand::GROUP:
    return plugin::Script::ScriptCommand::Group;
  case eld::ScriptCommand::INCLUDE:
    return plugin::Script::ScriptCommand::Include;
  case eld::ScriptCommand::INPUT:
    return plugin::Script::ScriptCommand::Input;
  case eld::ScriptCommand::INPUT_SECT_DESC:
    return plugin::Script::ScriptCommand::InputSectionSpec;
  case eld::ScriptCommand::NOCROSSREFS:
    return plugin::Script::ScriptCommand::NoCrossRefs;
  case eld::ScriptCommand::OUTPUT:
    return plugin::Script::ScriptCommand::Output;
  case eld::ScriptCommand::OUTPUT_ARCH:
    return plugin::Script::ScriptCommand::OutputArch;
  case eld::ScriptCommand::OUTPUT_FORMAT:
    return plugin::Script::ScriptCommand::OutputFormat;
  case eld::ScriptCommand::OUTPUT_SECT_DATA:
    return plugin::Script::ScriptCommand::OutputSectionData;
  case eld::ScriptCommand::OUTPUT_SECT_DESC:
    return plugin::Script::ScriptCommand::OutputSectionSpec;
  case eld::ScriptCommand::PLUGIN:
    return plugin::Script::ScriptCommand::Plugin;
  case eld::ScriptCommand::SEARCH_DIR:
    return plugin::Script::ScriptCommand::SearchDir;
  case eld::ScriptCommand::SECTIONS:
    return plugin::Script::ScriptCommand::Sections;
  default:
    break;
  }
  return plugin::Script::ScriptCommand::UnSupported;
}

plugin::Script::ScriptCommand *
plugin::Script::ScriptCommand::getScriptCommand(eld::ScriptCommand *SC) {
  switch (SC->getKind()) {
  case eld::ScriptCommand::PHDRS:
    return getPHDRS(SC);
  case eld::ScriptCommand::PHDR_DESC:
    return getPHDRDESC(SC);
  case eld::ScriptCommand::ASSIGNMENT:
    return getAssignment(SC);
  case eld::ScriptCommand::ENTER_SCOPE:
    return getEnterScope(SC);
  case eld::ScriptCommand::ENTRY:
    return getEntry(SC);
  case eld::ScriptCommand::EXIT_SCOPE:
    return getExitScope(SC);
  case eld::ScriptCommand::EXTERN:
    return getExtern(SC);
  case eld::ScriptCommand::GROUP:
    return getGroup(SC);
  case eld::ScriptCommand::INCLUDE:
    return getInclude(SC);
  case eld::ScriptCommand::INPUT:
    return getInput(SC);
  case eld::ScriptCommand::INPUT_SECT_DESC:
    return getInputSectionSpec(SC);
  case eld::ScriptCommand::NOCROSSREFS:
    return getNoCrossRefs(SC);
  case eld::ScriptCommand::OUTPUT:
    return getOutput(SC);
  case eld::ScriptCommand::OUTPUT_ARCH:
    return getOutputArch(SC);
  case eld::ScriptCommand::OUTPUT_FORMAT:
    return getOutputFormat(SC);
  case eld::ScriptCommand::OUTPUT_SECT_DATA:
    return getOutputSectionData(SC);
  case eld::ScriptCommand::OUTPUT_SECT_DESC:
    return getOutputSectionSpec(SC);
  case eld::ScriptCommand::PLUGIN:
    return getPlugin(SC);
  case eld::ScriptCommand::SEARCH_DIR:
    return getSearchDir(SC);
  case eld::ScriptCommand::SECTIONS:
    return getSections(SC);
  default:
    ASSERT(0, "Unhandled command");
    return new plugin::Script::ScriptCommand();
  }
}

uint32_t plugin::Script::ScriptCommand::getLevel() const {
  if (!getCommand())
    return 0;
  return getCommand()->getDepth();
}

plugin::Script::PHDRS *
plugin::Script::ScriptCommand::getPHDRS(eld::ScriptCommand *SC) {
  return new plugin::Script::PHDRS(llvm::dyn_cast<eld::PhdrsCmd>(SC));
}

plugin::Script::PHDRDescriptor *
plugin::Script::ScriptCommand::getPHDRDESC(eld::ScriptCommand *SC) {
  return new plugin::Script::PHDRDescriptor(llvm::dyn_cast<eld::PhdrDesc>(SC));
}

plugin::Script::Assignment *
plugin::Script::ScriptCommand::getAssignment(eld::ScriptCommand *SC) {
  return new plugin::Script::Assignment(llvm::dyn_cast<eld::Assignment>(SC));
}

plugin::Script::EnterScope *
plugin::Script::ScriptCommand::getEnterScope(eld::ScriptCommand *SC) {
  return new plugin::Script::EnterScope(llvm::dyn_cast<eld::EnterScopeCmd>(SC));
}

plugin::Script::Entry *
plugin::Script::ScriptCommand::getEntry(eld::ScriptCommand *SC) {
  return new plugin::Script::Entry(llvm::dyn_cast<eld::EntryCmd>(SC));
}

plugin::Script::ExitScope *
plugin::Script::ScriptCommand::getExitScope(eld::ScriptCommand *SC) {
  return new plugin::Script::ExitScope(llvm::dyn_cast<eld::ExitScopeCmd>(SC));
}

plugin::Script::Extern *
plugin::Script::ScriptCommand::getExtern(eld::ScriptCommand *SC) {
  return new plugin::Script::Extern(llvm::dyn_cast<eld::ExternCmd>(SC));
}

plugin::Script::Group *
plugin::Script::ScriptCommand::getGroup(eld::ScriptCommand *SC) {
  return new plugin::Script::Group(llvm::dyn_cast<eld::GroupCmd>(SC));
}

plugin::Script::Include *
plugin::Script::ScriptCommand::getInclude(eld::ScriptCommand *SC) {
  return new plugin::Script::Include(llvm::dyn_cast<eld::IncludeCmd>(SC));
}

plugin::Script::Input *
plugin::Script::ScriptCommand::getInput(eld::ScriptCommand *SC) {
  return new plugin::Script::Input(llvm::dyn_cast<eld::InputCmd>(SC));
}

plugin::Script::InputSectionSpec *
plugin::Script::ScriptCommand::getInputSectionSpec(eld::ScriptCommand *SC) {
  return new plugin::Script::InputSectionSpec(
      llvm::dyn_cast<eld::InputSectDesc>(SC));
}

plugin::Script::NoCrossRefs *
plugin::Script::ScriptCommand::getNoCrossRefs(eld::ScriptCommand *SC) {
  return new plugin::Script::NoCrossRefs(
      llvm::dyn_cast<eld::NoCrossRefsCmd>(SC));
}

plugin::Script::Output *
plugin::Script::ScriptCommand::getOutput(eld::ScriptCommand *SC) {
  return new plugin::Script::Output(llvm::dyn_cast<eld::OutputCmd>(SC));
}

plugin::Script::OutputArch *
plugin::Script::ScriptCommand::getOutputArch(eld::ScriptCommand *SC) {
  return new plugin::Script::OutputArch(llvm::dyn_cast<eld::OutputArchCmd>(SC));
}

plugin::Script::OutputFormat *
plugin::Script::ScriptCommand::getOutputFormat(eld::ScriptCommand *SC) {
  return new plugin::Script::OutputFormat(
      llvm::dyn_cast<eld::OutputFormatCmd>(SC));
}

plugin::Script::OutputSectionSpec *
plugin::Script::ScriptCommand::getOutputSectionSpec(eld::ScriptCommand *SC) {
  return new plugin::Script::OutputSectionSpec(
      llvm::dyn_cast<eld::OutputSectDesc>(SC));
}

plugin::Script::OutputSectionData *
plugin::Script::ScriptCommand::getOutputSectionData(eld::ScriptCommand *SC) {
  return new plugin::Script::OutputSectionData(
      llvm::cast<eld::OutputSectData>(SC));
}

plugin::Script::Plugin *
plugin::Script::ScriptCommand::getPlugin(eld::ScriptCommand *SC) {
  return new plugin::Script::Plugin(llvm::dyn_cast<eld::PluginCmd>(SC));
}

plugin::Script::SearchDir *
plugin::Script::ScriptCommand::getSearchDir(eld::ScriptCommand *SC) {
  return new plugin::Script::SearchDir(llvm::dyn_cast<eld::SearchDirCmd>(SC));
}

plugin::Script::Sections *
plugin::Script::ScriptCommand::getSections(eld::ScriptCommand *SC) {
  return new plugin::Script::Sections(llvm::dyn_cast<eld::SectionsCmd>(SC));
}

plugin::Script::PHDRS plugin::Script::ScriptCommand::getPHDRS() const {
  return plugin::Script::PHDRS(llvm::dyn_cast<eld::PhdrsCmd>(getCommand()));
}

plugin::Script::PHDRDescriptor
plugin::Script::ScriptCommand::getPHDRDESC() const {
  return plugin::Script::PHDRDescriptor(
      llvm::dyn_cast<eld::PhdrDesc>(getCommand()));
}

plugin::Script::Assignment
plugin::Script::ScriptCommand::getAssignment() const {
  return plugin::Script::Assignment(
      llvm::dyn_cast<eld::Assignment>(getCommand()));
}

plugin::Script::EnterScope
plugin::Script::ScriptCommand::getEnterScope() const {
  return plugin::Script::EnterScope(
      llvm::dyn_cast<eld::EnterScopeCmd>(getCommand()));
}

plugin::Script::Entry plugin::Script::ScriptCommand::getEntry() const {
  return plugin::Script::Entry(llvm::dyn_cast<eld::EntryCmd>(getCommand()));
}

plugin::Script::ExitScope plugin::Script::ScriptCommand::getExitScope() const {
  return plugin::Script::ExitScope(
      llvm::dyn_cast<eld::ExitScopeCmd>(getCommand()));
}

plugin::Script::Extern plugin::Script::ScriptCommand::getExtern() const {
  return plugin::Script::Extern(llvm::dyn_cast<eld::ExternCmd>(getCommand()));
}

plugin::Script::Group plugin::Script::ScriptCommand::getGroup() const {
  return plugin::Script::Group(llvm::dyn_cast<eld::GroupCmd>(getCommand()));
}

plugin::Script::Include plugin::Script::ScriptCommand::getInclude() const {
  return plugin::Script::Include(llvm::dyn_cast<eld::IncludeCmd>(getCommand()));
}

plugin::Script::Input plugin::Script::ScriptCommand::getInput() const {
  return plugin::Script::Input(llvm::dyn_cast<eld::InputCmd>(getCommand()));
}

plugin::Script::InputSectionSpec
plugin::Script::ScriptCommand::getInputSectionSpec() const {
  return plugin::Script::InputSectionSpec(
      llvm::dyn_cast<eld::InputSectDesc>(getCommand()));
}

plugin::Script::NoCrossRefs
plugin::Script::ScriptCommand::getNoCrossRefs() const {
  return plugin::Script::NoCrossRefs(
      llvm::dyn_cast<eld::NoCrossRefsCmd>(getCommand()));
}

plugin::Script::Output plugin::Script::ScriptCommand::getOutput() const {
  return plugin::Script::Output(llvm::dyn_cast<eld::OutputCmd>(getCommand()));
}

plugin::Script::OutputArch
plugin::Script::ScriptCommand::getOutputArch() const {
  return plugin::Script::OutputArch(
      llvm::dyn_cast<eld::OutputArchCmd>(getCommand()));
}

plugin::Script::OutputFormat
plugin::Script::ScriptCommand::getOutputFormat() const {
  return plugin::Script::OutputFormat(
      llvm::dyn_cast<eld::OutputFormatCmd>(getCommand()));
}

plugin::Script::OutputSectionSpec
plugin::Script::ScriptCommand::getOutputSectionSpec() const {
  return plugin::Script::OutputSectionSpec(
      llvm::dyn_cast<eld::OutputSectDesc>(getCommand()));
}

plugin::Script::Plugin plugin::Script::ScriptCommand::getPlugin() const {
  return plugin::Script::Plugin(llvm::dyn_cast<eld::PluginCmd>(getCommand()));
}

plugin::Script::SearchDir plugin::Script::ScriptCommand::getSearchDir() const {
  return plugin::Script::SearchDir(
      llvm::dyn_cast<eld::SearchDirCmd>(getCommand()));
}

plugin::Script::Sections plugin::Script::ScriptCommand::getSections() const {
  return plugin::Script::Sections(
      llvm::dyn_cast<eld::SectionsCmd>(getCommand()));
}

std::string plugin::Script::ScriptCommand::toString() const {
  eld::ScriptCommand *Cmd = getCommand();
  if (!Cmd)
    return std::string();
  std::string SS;
  llvm::raw_string_ostream OSS(SS);
  Cmd->dumpOnlyThis(OSS);
  return SS;
}

std::vector<plugin::Script::ScriptCommand *>
plugin::Script::ScriptCommand::getCommands() const {
  std::vector<plugin::Script::ScriptCommand *> Empty;
  return Empty;
}

std::string plugin::Script::ScriptCommand::getOrigin() const {
  return getCommand()->getContext();
}

//
// PHDRDescriptor
//
plugin::Script::PHDRDescriptor::PHDRDescriptor(eld::PhdrDesc *PhdrDesc)
    : m_PhdrDesc(PhdrDesc) {}

const std::string plugin::Script::PHDRDescriptor::getSegmentName() const {
  return m_PhdrDesc->getPhdrSpec()->name();
}

const std::string plugin::Script::PHDRDescriptor::getSegmentType() const {
  return eld::ELFSegment::TypeToELFTypeStr(m_PhdrDesc->getPhdrSpec()->type())
      .str();
}

bool plugin::Script::PHDRDescriptor::hasSegmentFlags() const {
  return m_PhdrDesc->getPhdrSpec()->flags();
}

std::string plugin::Script::PHDRDescriptor::getSegmentFlags() const {
  if (!m_PhdrDesc->getPhdrSpec()->flags())
    return "";
  m_PhdrDesc->getPhdrSpec()->flags()->evaluateAndRaiseError();
  return std::string("0x") +
         eld::utility::toHex(m_PhdrDesc->getPhdrSpec()->flags()->result());
}

eld::ScriptCommand *plugin::Script::PHDRDescriptor::getCommand() const {
  return m_PhdrDesc;
}

//
// PHDRS
//
plugin::Script::PHDRS::PHDRS(eld::PhdrsCmd *PhdrsCmd)
    : ScriptCommand(plugin::Script::ScriptCommand::PHDRS),
      m_PhdrsCmd(PhdrsCmd) {
  getPhdrDescriptors();
}

eld::ScriptCommand *plugin::Script::PHDRS::getCommand() const {
  return m_PhdrsCmd;
}

void plugin::Script::PHDRS::getPhdrDescriptors() {
  if (!m_PhdrsCmd)
    return;
  for (auto &LSC : m_PhdrsCmd->getPhdrDescriptors()) {
    plugin::Script::ScriptCommand *Cmd =
        plugin::Script::ScriptCommand::getScriptCommand(LSC);
    m_ScriptCommands.push_back(Cmd);
  }
}

std::vector<plugin::Script::ScriptCommand *>
plugin::Script::PHDRS::getCommands() const {
  return m_ScriptCommands;
}

plugin::Script::PHDRS::~PHDRS() {
  for (auto &Cmd : m_ScriptCommands)
    delete Cmd;
}

//
// Assignment
//
plugin::Script::Assignment::Assignment(eld::Assignment *Assignment)
    : ScriptCommand(plugin::Script::ScriptCommand::Assignment),
      m_Assignment(Assignment) {}

eld::ScriptCommand *plugin::Script::Assignment::getCommand() const {
  return m_Assignment;
}

bool plugin::Script::Assignment::isGlobal() const {
  return m_Assignment->isOutsideSections();
}

bool plugin::Script::Assignment::isOutsideOutputSection() const {
  return m_Assignment->isOutsideOutputSection();
}

bool plugin::Script::Assignment::isInsideOutputSection() const {
  return m_Assignment->isInsideOutputSection();
}

bool plugin::Script::Assignment::isProvide() const {
  return m_Assignment->isProvide();
}

bool plugin::Script::Assignment::isProvideHidden() const {
  return m_Assignment->isProvideHidden();
}

bool plugin::Script::Assignment::isFill() const {
  return m_Assignment->isFill();
}

bool plugin::Script::Assignment::isAssert() const {
  return m_Assignment->isAssert();
}

//
// EnterScope
//
plugin::Script::EnterScope::EnterScope(eld::EnterScopeCmd *EnterScope)
    : ScriptCommand(plugin::Script::ScriptCommand::EnterScope),
      m_EnterScope(EnterScope) {}

eld::ScriptCommand *plugin::Script::EnterScope::getCommand() const {
  return m_EnterScope;
}

//
// Entry
//
plugin::Script::Entry::Entry(eld::EntryCmd *Entry)
    : ScriptCommand(plugin::Script::ScriptCommand::Entry), m_Entry(Entry) {}

eld::ScriptCommand *plugin::Script::Entry::getCommand() const {
  return m_Entry;
}

//
// ExitScope
//
plugin::Script::ExitScope::ExitScope(eld::ExitScopeCmd *ExitScope)
    : ScriptCommand(plugin::Script::ScriptCommand::ExitScope),
      m_ExitScope(ExitScope) {}

eld::ScriptCommand *plugin::Script::ExitScope::getCommand() const {
  return m_ExitScope;
}

//
// Extern
//
plugin::Script::Extern::Extern(eld::ExternCmd *Extern)
    : ScriptCommand(plugin::Script::ScriptCommand::Extern), m_Extern(Extern) {}

eld::ScriptCommand *plugin::Script::Extern::getCommand() const {
  return m_Extern;
}

//
// Group
//
plugin::Script::Group::Group(eld::GroupCmd *Group)
    : ScriptCommand(plugin::Script::ScriptCommand::Group), m_Group(Group) {}

eld::ScriptCommand *plugin::Script::Group::getCommand() const {
  return m_Group;
}

//
// INCLUDE
//
plugin::Script::Include::Include(eld::IncludeCmd *Include)
    : ScriptCommand(plugin::Script::ScriptCommand::Include),
      m_Include(Include) {}

eld::ScriptCommand *plugin::Script::Include::getCommand() const {
  return m_Include;
}

bool plugin::Script::Include::isOptional() const {
  return m_Include->isOptional();
}

std::string plugin::Script::Include::getFileName() const {
  return m_Include->getFileName();
}

//
// INPUT
//
plugin::Script::Input::Input(eld::InputCmd *Input)
    : ScriptCommand(plugin::Script::ScriptCommand::Input), m_Input(Input) {}

eld::ScriptCommand *plugin::Script::Input::getCommand() const {
  return m_Input;
}

//
// INPUT_SECTION_SPEC
//
plugin::Script::InputSectionSpec::InputSectionSpec(
    eld::InputSectDesc *InputSectionSpec)
    : ScriptCommand(plugin::Script::ScriptCommand::InputSectionSpec),
      m_InputSectionSpec(InputSectionSpec) {}

eld::ScriptCommand *plugin::Script::InputSectionSpec::getCommand() const {
  return m_InputSectionSpec;
}

plugin::LinkerScriptRule
plugin::Script::InputSectionSpec::getLinkerScriptRule() const {
  return plugin::LinkerScriptRule(m_InputSectionSpec->getRuleContainer());
}

uint32_t plugin::Script::InputSectionSpec::getNumRuleMatches() const {
  if (!*this)
    return 0;
  eld::RuleContainer *RC = m_InputSectionSpec->getRuleContainer();
  if (!RC)
    return 0;
  return RC->getMatchCount();
}

uint32_t plugin::Script::InputSectionSpec::getMatchTime() const {
  if (!*this)
    return 0;
  eld::RuleContainer *RC = m_InputSectionSpec->getRuleContainer();
  if (!RC)
    return 0;
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(RC->getMatchTime())
          .count());
}

bool plugin::Script::InputSectionSpec::isInternal() const {
  return m_InputSectionSpec->isSpecial();
}

uint64_t plugin::Script::InputSectionSpec::getHash() const {
  if (!*this)
    return 0;
  return m_InputSectionSpec->getRuleHash();
}

std::string
plugin::Script::InputSectionSpec::getAsString(bool useNewLine, bool withValues,
                                              bool addIndent) const {
  std::string outs;
  llvm::raw_string_ostream ostream(outs);
  m_InputSectionSpec->dumpMap(ostream, /*useColor*/ false, useNewLine,
                              withValues, addIndent);
  return outs;
}

plugin::Script::InputSectionSpec::~InputSectionSpec() {}

//
// NOCROSSREFS
//
plugin::Script::NoCrossRefs::NoCrossRefs(eld::NoCrossRefsCmd *NoCrossRefs)
    : ScriptCommand(plugin::Script::ScriptCommand::NoCrossRefs),
      m_NoCrossRefs(NoCrossRefs) {}

eld::ScriptCommand *plugin::Script::NoCrossRefs::getCommand() const {
  return m_NoCrossRefs;
}

//
// OUTPUT
//
plugin::Script::Output::Output(eld::OutputCmd *Output)
    : ScriptCommand(plugin::Script::ScriptCommand::Output), m_Output(Output) {}

eld::ScriptCommand *plugin::Script::Output::getCommand() const {
  return m_Output;
}


//
// OUTPUT_ARCH
//
plugin::Script::OutputArch::OutputArch(eld::OutputArchCmd *OutputArch)
    : ScriptCommand(plugin::Script::ScriptCommand::OutputArch),
      m_OutputArch(OutputArch) {}

eld::ScriptCommand *plugin::Script::OutputArch::getCommand() const {
  return m_OutputArch;
}

//
// OUTPUT_FORMAT
//
plugin::Script::OutputFormat::OutputFormat(eld::OutputFormatCmd *OutputFormat)
    : ScriptCommand(plugin::Script::ScriptCommand::OutputFormat),
      m_OutputFormat(OutputFormat) {}

eld::ScriptCommand *plugin::Script::OutputFormat::getCommand() const {
  return m_OutputFormat;
}

//
// OUTPUT_SECTION_DESC
//
plugin::Script::OutputSectionSpec::OutputSectionSpec(
    eld::OutputSectDesc *OutputSectionSpec)
    : ScriptCommand(plugin::Script::ScriptCommand::OutputSectionSpec),
      m_OutputSectionSpec(OutputSectionSpec) {
  getOutputSectionSpecCommands();
}

eld::ScriptCommand *plugin::Script::OutputSectionSpec::getCommand() const {
  return m_OutputSectionSpec;
}

void plugin::Script::OutputSectionSpec::getOutputSectionSpecCommands() {
  for (auto &LSC : m_OutputSectionSpec->getOutputSectCommands()) {
    plugin::Script::ScriptCommand *Cmd =
        plugin::Script::ScriptCommand::getScriptCommand(LSC);
    m_OutputSectionSpecCommands.push_back(Cmd);
  }
}

plugin::Script::OutputSectionSpec::~OutputSectionSpec() {
  for (auto &S : m_OutputSectionSpecCommands)
    delete S;
}

std::vector<plugin::Script::ScriptCommand *>
plugin::Script::OutputSectionSpec::getCommands() const {
  return m_OutputSectionSpecCommands;
}

///
/// PLUGIN_CONTROL_FILESZ
/// PLUGIN_CONTROL_MEMSZ
/// PLUGIN_ITER_SECTIONS
/// PLUGIN_SECTION_MATCHER
/// PLUGIN_OUTPUT_SECTION_ITER
/// LINKER_PLUGIN
//
plugin::Script::Plugin::Plugin(eld::PluginCmd *Plugin)
    : ScriptCommand(plugin::Script::ScriptCommand::Plugin), m_Plugin(Plugin) {}

eld::ScriptCommand *plugin::Script::Plugin::getCommand() const {
  return m_Plugin;
}

//
// SEARCH_DIR
//
plugin::Script::SearchDir::SearchDir(eld::SearchDirCmd *SearchDir)
    : ScriptCommand(plugin::Script::ScriptCommand::SearchDir),
      m_SearchDir(SearchDir) {}

eld::ScriptCommand *plugin::Script::SearchDir::getCommand() const {
  return m_SearchDir;
}

//
// SECTIONS
//
plugin::Script::Sections::Sections(eld::SectionsCmd *Sections)
    : ScriptCommand(plugin::Script::ScriptCommand::Sections),
      m_Sections(Sections) {
  getSectionCmdContents();
}

eld::ScriptCommand *plugin::Script::Sections::getCommand() const {
  return m_Sections;
}

void plugin::Script::Sections::getSectionCmdContents() {
  for (auto &LSC : m_Sections->getSectionCommands()) {
    plugin::Script::ScriptCommand *Cmd =
        plugin::Script::ScriptCommand::getScriptCommand(LSC);
    m_SectionCommands.push_back(Cmd);
  }
}

plugin::Script::Sections::~Sections() {
  for (auto &S : m_SectionCommands)
    delete S;
}

std::vector<plugin::Script::ScriptCommand *>
plugin::Script::Sections::getCommands() const {
  return m_SectionCommands;
}

//
// LinkerScript
//
plugin::LinkerScript::LinkerScript(eld::LinkerScript *LinkerScript)
    : m_LinkerScript(LinkerScript) {
  getLinkerScriptCommands();
}

plugin::LinkerScript::~LinkerScript() {
  for (auto &Cmd : m_ScriptCommands)
    delete Cmd;
}

void plugin::LinkerScript::getLinkerScriptCommands() {
  if (!m_LinkerScript)
    return;
  for (auto &LSC : m_LinkerScript->getScriptCommands()) {
    if (LSC->getKind() == eld::ScriptCommand::PLUGIN) {
      auto Cmd = llvm::dyn_cast<eld::PluginCmd>(LSC);
      if (Cmd->hasOutputSection())
        continue;
    }
    plugin::Script::ScriptCommand *Cmd =
        plugin::Script::ScriptCommand::getScriptCommand(LSC);
    m_ScriptCommands.push_back(Cmd);
  }
}

std::vector<plugin::Script::ScriptCommand *> &
plugin::LinkerScript::getCommands() {
  return m_ScriptCommands;
}

bool plugin::LinkerScript::hasSectionsCommand() const {
  return m_LinkerScript && m_LinkerScript->linkerScriptHasSectionsCommand();
}

bool plugin::LinkerScript::linkerScriptHasRules() {
  if (!m_LinkerScript)
    return false;
  return m_LinkerScript->linkerScriptHasRules();
}

std::string plugin::LinkerScript::getHash() const {
  if (!m_LinkerScript)
    return {};
  return m_LinkerScript->getHash();
}

plugin::Script::OutputSectionData::OutputSectionData(
    eld::OutputSectData *outputSectData)
    : ScriptCommand(plugin::Script::ScriptCommand::OutputSectionData),
      m_OutputSectData(outputSectData) {}

eld::ScriptCommand *plugin::Script::OutputSectionData::getCommand() const {
  return m_OutputSectData;
}
