//===- LinkerScript.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_PLUGINAPI_LINKERSCRIPT_H
#define ELD_PLUGINAPI_LINKERSCRIPT_H

#include "Defines.h"
#include <ostream>
#include <string>
#include <vector>

/// Forward declarations of internal linker data structures.
namespace eld {
class Assignment;
class EnterScopeCmd;
class EntryCmd;
class ExitScopeCmd;
class ExternCmd;
class GroupCmd;
class IncludeCmd;
class InputCmd;
class InputSectDesc;
class LinkerScript;
class NoCrossRefsCmd;
class OutputCmd;
class OutputArchCmd;
class OutputArchOptionCmd;
class OutputFormatCmd;
class OutputSectData;
class OutputSectDesc;
class PhdrDesc;
class PhdrsCmd;
class PluginCmd;
class Script;
class ScriptCommand;
class SearchDirCmd;
class SectionsCmd;
} // namespace eld

namespace eld::plugin {

struct DLL_A_EXPORT LinkerScriptRule;

namespace Script {

/// Forward Declarations.
struct DLL_A_EXPORT PHDRS;
struct DLL_A_EXPORT PHDRDescriptor;
struct DLL_A_EXPORT Assignment;
struct DLL_A_EXPORT EnterScope;
struct DLL_A_EXPORT Entry;
struct DLL_A_EXPORT ExitScope;
struct DLL_A_EXPORT Extern;
struct DLL_A_EXPORT Group;
struct DLL_A_EXPORT Include;
struct DLL_A_EXPORT Input;
struct DLL_A_EXPORT InputSectionSpec;
struct DLL_A_EXPORT NoCrossRefs;
struct DLL_A_EXPORT Output;
struct DLL_A_EXPORT OutputArchOption;
struct DLL_A_EXPORT OutputArch;
struct DLL_A_EXPORT OutputFormat;
struct DLL_A_EXPORT OutputSectionData;
struct DLL_A_EXPORT OutputSectionSpec;
struct DLL_A_EXPORT Plugin;
struct DLL_A_EXPORT SearchDir;
struct DLL_A_EXPORT Sections;

/// \class ScriptCommand
/// \brief Base class for all linker script commands.
struct DLL_A_EXPORT ScriptCommand {
  /// \enum CommmandKind - represents the kind of commands supported
  enum CommandKind : uint8_t {
    Assignment,
    EnterScope,
    Entry,
    ExitScope,
    Extern,
    Group,
    Include,
    Input,
    InputSectionSpec,
    NoCrossRefs,
    Output,
    OutputArch,
    OutputArchOption,
    OutputFormat,
    OutputSectionData,
    OutputSectionSpec,
    Plugin,
    PHDRS,
    PHDRDesc,
    SearchDir,
    Sections,
    UnSupported
  };

  /// Quick Helper functions
  bool isAssignment() const { return m_Kind == Assignment; }

  bool isEnterScope() const { return m_Kind == EnterScope; }

  bool isEntry() const { return m_Kind == Entry; }

  bool isExitScope() const { return m_Kind == ExitScope; }

  bool isExtern() const { return m_Kind == Extern; }

  bool isGroup() const { return m_Kind == Group; }

  bool isInput() const { return m_Kind == Input; }

  bool isInputSectionSpec() const { return m_Kind == InputSectionSpec; }

  bool isNoCrossRefs() const { return m_Kind == NoCrossRefs; }

  bool isOutput() const { return m_Kind == Output; }

  bool isOutputArch() const { return m_Kind == OutputArch; }

  bool isOutputArchOption() const { return m_Kind == OutputArchOption; }

  bool isOutputFormat() const { return m_Kind == OutputFormat; }

  bool isPlugin() const { return m_Kind == Plugin; }

  bool isPHDRS() const { return m_Kind == PHDRS; }

  bool isPHDRDesc() const { return m_Kind == PHDRDesc; }

  bool isSearchDir() const { return m_Kind == SearchDir; }

  bool isSections() const { return m_Kind == Sections; }

  bool isInclude() const { return m_Kind == Include; }

  bool isUnSupported() const { return m_Kind == UnSupported; }

  /// \returns the type of command that is represented.
  CommandKind getCommandKind() const { return m_Kind; }

  /// \returns the Command kind from a internal ScriptCommand
  static CommandKind getCommandKind(eld::ScriptCommand *SC);

  /// Constructor
  ScriptCommand(CommandKind K) : m_Kind(K) {}

  /// Constructor
  ScriptCommand() : m_Kind(CommandKind::UnSupported) {}

  /// \returns the internal ScriptCommand object.
  virtual eld::ScriptCommand *getCommand() const { return nullptr; }

  /// \returns the Command to a std::string
  virtual std::string toString() const;

  /// \returns the Level of the ScriptCommand.
  virtual uint32_t getLevel() const;

  /// Destructor
  virtual ~ScriptCommand() {}

  /// ostream operator to dump the object to ostream
  friend std::ostream &operator<<(std::ostream &OS,
                                  const plugin::Script::ScriptCommand &Cmd) {
    OS << Cmd.toString();
    return OS;
  }

  /// ostream operator to dump the object to ostream
  friend std::ostream &operator<<(std::ostream &OS,
                                  const plugin::Script::ScriptCommand *Cmd) {
    OS << Cmd->toString();
    return OS;
  }

  /// \returns a vector of ScriptCommand's for this ScriptCommand.
  virtual std::vector<plugin::Script::ScriptCommand *> getCommands() const;

  /// \returns if the Command has additional commands.
  virtual bool hasMoreCommands() const { return false; }

  /// \returns the equivalent ScriptCommand for the Command.
  static ScriptCommand *getScriptCommand(eld::ScriptCommand *SC);

  /// \returns the linker script command PHDRS.
  static plugin::Script::PHDRS *getPHDRS(eld::ScriptCommand *SC);

  /// \returns the linker script command PHDR_DESC
  static plugin::Script::PHDRDescriptor *getPHDRDESC(eld::ScriptCommand *SC);

  /// \returns the linker script command Assignment.
  static plugin::Script::Assignment *getAssignment(eld::ScriptCommand *SC);

  /// \returns the linker script command ENTER_SCOPE.
  static plugin::Script::EnterScope *getEnterScope(eld::ScriptCommand *SC);

  /// \returns the linker script command ENTRY.
  static plugin::Script::Entry *getEntry(eld::ScriptCommand *SC);

  /// \returns the linker script command EXIT_SCOPE.
  static plugin::Script::ExitScope *getExitScope(eld::ScriptCommand *SC);

  /// \returns the linker script command EXTERN.
  static plugin::Script::Extern *getExtern(eld::ScriptCommand *SC);

  /// \returns the linker script command GROUP.
  static plugin::Script::Group *getGroup(eld::ScriptCommand *SC);

  /// \returns the linker script command INPUT.
  static plugin::Script::Include *getInclude(eld::ScriptCommand *SC);

  /// \returns the linker script command INPUT.
  static plugin::Script::Input *getInput(eld::ScriptCommand *SC);

  /// \returns the linker script command INPUT_SECT_DESC.
  static plugin::Script::InputSectionSpec *
  getInputSectionSpec(eld::ScriptCommand *SC);

  /// \returns the linker script command NOCROSSREFS.
  static plugin::Script::NoCrossRefs *getNoCrossRefs(eld::ScriptCommand *SC);

  /// \returns the linker script command OUTPUT.
  static plugin::Script::Output *getOutput(eld::ScriptCommand *SC);

  /// \returns the linker script command OUTPUT_ARCH.
  static plugin::Script::OutputArch *getOutputArch(eld::ScriptCommand *SC);

  /// \returns the linker script command OUTPUT_FORMAT.
  static plugin::Script::OutputFormat *getOutputFormat(eld::ScriptCommand *SC);

  /// \returns the linker script command OUTPUT_SECT_DESC.
  static plugin::Script::OutputSectionSpec *
  getOutputSectionSpec(eld::ScriptCommand *SC);

  /// \returns linker script output section data command.
  /// These commands include SHORT, BYTE, LONG, QUAD and SQUAD.
  static plugin::Script::OutputSectionData *
  getOutputSectionData(eld::ScriptCommand *SC);

  /// \returns the linker script command PLUGIN.
  static plugin::Script::Plugin *getPlugin(eld::ScriptCommand *SC);

  /// \returns the linker script command SEARCH_DIR.
  static plugin::Script::SearchDir *getSearchDir(eld::ScriptCommand *SC);

  /// \returns the linker script command SECTIONS.
  static plugin::Script::Sections *getSections(eld::ScriptCommand *SC);

  /// \returns the linker script command PHDRS.
  plugin::Script::PHDRS getPHDRS() const;

  /// \returns the linker script command PHDR_DESC
  plugin::Script::PHDRDescriptor getPHDRDESC() const;

  /// \returns the linker script command Assignment.
  plugin::Script::Assignment getAssignment() const;

  /// \returns the linker script command ENTER_SCOPE.
  plugin::Script::EnterScope getEnterScope() const;

  /// \returns the linker script command ENTRY.
  plugin::Script::Entry getEntry() const;

  /// \returns the linker script command EXIT_SCOPE.
  plugin::Script::ExitScope getExitScope() const;

  /// \returns the linker script command EXTERN.
  plugin::Script::Extern getExtern() const;

  /// \returns the linker script command GROUP.
  plugin::Script::Group getGroup() const;

  /// \returns the linker script command INCLUDE.
  plugin::Script::Include getInclude() const;

  /// \returns the linker script command INPUT.
  plugin::Script::Input getInput() const;

  /// \returns the linker script command INPUT_SECT_DESC.
  plugin::Script::InputSectionSpec getInputSectionSpec() const;

  /// \returns the linker script command NOCROSSREFS.
  plugin::Script::NoCrossRefs getNoCrossRefs() const;

  /// \returns the linker script command OUTPUT.
  plugin::Script::Output getOutput() const;

  /// \returns the linker script command OUTPUT_ARCH_OPTION.
  plugin::Script::OutputArchOption getOutputArchOption() const;

  /// \returns the linker script command OUTPUT_ARCH.
  plugin::Script::OutputArch getOutputArch() const;

  /// \returns the linker script command OUTPUT_FORMAT.
  plugin::Script::OutputFormat getOutputFormat() const;

  /// \returns the linker script command OUTPUT_SECT_DESC.
  plugin::Script::OutputSectionSpec getOutputSectionSpec() const;

  /// \returns the linker script command PLUGIN.
  plugin::Script::Plugin getPlugin() const;

  /// \returns the linker script command SEARCH_DIR.
  plugin::Script::SearchDir getSearchDir() const;

  /// \returns the linker script command SECTIONS.
  plugin::Script::Sections getSections() const;

  /// \returns the Origin of the Rule.
  std::string getOrigin() const;

private:
  CommandKind m_Kind;
};

/// \class PHDRDescriptor
/// \brief This class represents each segment and the corresponding members
struct DLL_A_EXPORT PHDRDescriptor : public ScriptCommand {
  /// Constructor
  explicit PHDRDescriptor(eld::PhdrDesc *PhdrDesc);

  /// \returns true/false - Allow object to be used in a condition.
  operator bool() const { return m_PhdrDesc != nullptr; }

  /// \returns the name of the segment.
  const std::string getSegmentName() const;

  /// \returns Get the Type of the segment.
  const std::string getSegmentType() const;

  /// \returns true/false depending on whether the user specified
  /// input flags
  bool hasSegmentFlags() const;

  /// \returns Segment flags specified
  std::string getSegmentFlags() const;

  /// Destructor
  virtual ~PHDRDescriptor() {}

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

public:
  /// Phdr Spec.
  eld::PhdrDesc *m_PhdrDesc = nullptr;
};

/// \class PHDRS
/// \brief This class represents the PHDR spec defined in the Linker script
/// \brief Specification :-
///  PHDRS
///  {
///       name type [ FILEHDR ] [ PHDRS ] [ AT ( address ) ]
///             [ FLAGS ( flags ) ] ;
///  }
struct DLL_A_EXPORT PHDRS : public ScriptCommand {
  /// Constructor
  explicit PHDRS(eld::PhdrsCmd *PhdrsCmd);

  /// \returns true/false - Allow object to be used in a condition.
  operator bool() const { return m_PhdrsCmd != nullptr; }

  /// Destructor
  virtual ~PHDRS();

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// \returns if the Command has additional commands.
  bool hasMoreCommands() const override { return true; }

  /// \returns Phdr Descriptors
  std::vector<plugin::Script::ScriptCommand *> getCommands() const override;

private:
  void getPhdrDescriptors();

private:
  eld::PhdrsCmd *m_PhdrsCmd = nullptr;
  std::vector<plugin::Script::ScriptCommand *> m_ScriptCommands;
};

/// \class Assignment
/// \brief Handle Symbol Assignments, ASSERT, PRINT, FILL, PROVIDE,
/// PROVIDE_HIDDEN
///
/// \brief Specification :-
/// You may create global symbols, and assign values (addresses) to global
/// symbols, using any of the C assignment operators: symbol = expression ;
/// symbol &= expression ;
/// symbol += expression ;
/// symbol -= expression ;
/// symbol *= expression ;
/// symbol /= expression ;
struct DLL_A_EXPORT Assignment : public ScriptCommand {
  explicit Assignment(eld::Assignment *Assignment);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Assignment != nullptr; }

  /// \returns true if a assignment is global.
  bool isGlobal() const;

  /// \returns true if the assignment is specified outside an output section.
  bool isOutsideOutputSection() const;

  /// \returns true if the assignment is inside an output section
  bool isInsideOutputSection() const;

  /// \returns true if the assignment uses PROVIDE
  bool isProvide() const;

  /// \returns true if the assignment is PROVIDE_HIDDEN
  bool isProvideHidden() const;

  /// \returns true if the assignment is a FILL
  bool isFill() const;

  /// \returns true if the assignment is a ASSERT
  bool isAssert() const;

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Assignment() {}

public:
  eld::Assignment *m_Assignment = nullptr;
};

/// \class EnterScope
/// \brief Handle EnterScope command
///
/// \brief Specification :- represented by {
struct DLL_A_EXPORT EnterScope : public ScriptCommand {
  explicit EnterScope(eld::EnterScopeCmd *EnterScope);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_EnterScope != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~EnterScope() {}

public:
  eld::EnterScopeCmd *m_EnterScope = nullptr;
};

/// \class ExitScope
/// \brief Handle ExitScope command
///
/// \brief Specification :- represented by }
struct DLL_A_EXPORT ExitScope : public ScriptCommand {
  explicit ExitScope(eld::ExitScopeCmd *ExitScope);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_ExitScope != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~ExitScope() {}

public:
  eld::ExitScopeCmd *m_ExitScope = nullptr;
};

/// \class Entry
/// \brief Handle Entry command
///
/// \brief Specification :-

/// The linker command language includes a command specifically for defining
/// the first executable instruction in an output file (its entry point). Its
/// argument is a symbol name:
///
/// ENTRY(symbol) Like symbol assignments, the ENTRY command may be placed
/// either as an independent command in the command file, or among the section
/// definitions within the SECTIONS command--whatever makes the most sense for
/// your layout.
///
/// ENTRY is only one of several ways of choosing the entry point. You may
/// indicate it in any of the following ways (shown in descending order of
/// priority: methods higher in the list override methods lower down).
///
/// * The `-e' entry command-line option;
/// * The ENTRY(symbol) command in a linker control script;
/// * The value of the symbol start, if present;
/// * The address of the first byte of the .text section, if present;
/// * The address 0.

struct DLL_A_EXPORT Entry : public ScriptCommand {
  explicit Entry(eld::EntryCmd *Entry);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Entry != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Entry() {}

public:
  eld::EntryCmd *m_Entry = nullptr;
};

/// \class EXTERN
/// \brief Handle Extern Command
///
/// \brief Specification :-
/// EXTERN(symbol symbol ...)
/// Force symbol to be entered in the output file as an undefined symbol.
/// Doing this may, for example, trigger linking of additional modules from
/// standard libraries. You may list several symbols for each EXTERN, and you
/// may use EXTERN multiple times. This command has the same effect as the `-u'
/// command-line option.
struct DLL_A_EXPORT Extern : public ScriptCommand {
  explicit Extern(eld::ExternCmd *Extern);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Extern != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Extern() {}

public:
  eld::ExternCmd *m_Extern = nullptr;
};

/// \class GROUP
/// \brief Handle Group Command
///
/// \brief Specification :-
/// GROUP ( file, file, ... )
/// GROUP ( file file ... )
/// This command is like INPUT, except that the named files should all be
/// archives, and they are searched repeatedly until no new undefined references
/// are created.

struct DLL_A_EXPORT Group : public ScriptCommand {
  explicit Group(eld::GroupCmd *Group);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Group != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Group() {}

public:
  eld::GroupCmd *m_Group = nullptr;
};

/// \class Include
/// \brief Handle INCLUDE/INCLUDE_OPTIONAL Command
///
/// INCLUDE filename
/// INCLUDE_OPTIONAL filename
/// Include the linker script filename at this point. The
/// file will be searched for in the current directory, and in any directory
/// specified with the -L option. You can nest calls to INCLUDE up to 10 levels
/// deep You can place INCLUDE directives at the top level, in MEMORY or
/// SECTIONS commands, or in output section descriptions
struct DLL_A_EXPORT Include : public ScriptCommand {
  explicit Include(eld::IncludeCmd *Include);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Include != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// \returns if the INCLUDE command specifies INCLUDE_OPTIONAL
  bool isOptional() const;

  /// \returns the filename specified for the INCLUDE command.
  std::string getFileName() const;

  /// Destructor
  virtual ~Include() {}

public:
  eld::IncludeCmd *m_Include = nullptr;
};

/// \class INPUT
/// \brief Handle Input Command
///
/// INPUT(file, file, ...)
/// INPUT(file file ...)
/// The INPUT command directs the linker to include the named files in the     .
/// link, as though they were named on the command line For example, if you    .
/// always want to include subr.o any time you do a link, but you can't be     .
/// bothered to put it on every link command line, then you can put `INPUT     .
/// (subr.o)' in your linker script                                            .
///
/// In fact, if you like, you can list all of your input files in the linker
/// script, and then invoke the linker with nothing but a `-T' option.
///
/// In case a sysroot prefix is configured, and the filename starts with the `/'
/// character, and the script being processed was located inside the sysroot
/// prefix, the filename will be looked for in the sysroot prefix. Otherwise,
/// the linker will try to open the file in the current directory. If it is not
/// found, the linker will search through the archive library search path. The
/// sysroot prefix can also be forced by specifying = as the first character in
/// the filename path.

/// If you use `INPUT (-lfile)', ld will transform the name to libfile.a, as
/// with the command line argument `-l'.
struct DLL_A_EXPORT Input : public ScriptCommand {
  explicit Input(eld::InputCmd *Input);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Input != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Input() {}

public:
  eld::InputCmd *m_Input = nullptr;
};

/// \class InputSectionSpec
/// \brief Handle InputSection Specification
/// The input section description is the most basic linker script operation.
/// You use output sections to tell the linker how to lay out your program in
/// memory. You use input section descriptions to tell the linker how to map the
/// input files into your memory layout.
struct DLL_A_EXPORT InputSectionSpec : public ScriptCommand {
  explicit InputSectionSpec(eld::InputSectDesc *ISD);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_InputSectionSpec != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  std::string getAsString(bool useNewLine, bool withValues,
                          bool addIndent) const;

  /// \returns the Linker script rule defined by the Input Section spec.
  plugin::LinkerScriptRule getLinkerScriptRule() const;

  virtual ~InputSectionSpec();

  /// \returns the number of sections that the Rule matched.
  uint32_t getNumRuleMatches() const;

  /// \returns the hash of the linkerscript rule defined by the input section
  /// spec
  uint64_t getHash() const;

  /// \returns the total time taken by the Linker for matching.
  uint32_t getMatchTime() const;

  /// \returns true/false if the Rule was internal to the Linker.
  bool isInternal() const;

public:
  eld::InputSectDesc *m_InputSectionSpec = nullptr;
};

/// \class NOCROSSREFS
/// \brief Handle NOCROSSREFS Command
/// NOCROSSREFS(section section )
/// This command may be used to tell ld to issue an error about any references
/// among certain output sections In certain types of programs, particularly
/// on embedded systems when using overlays, when one section is loaded into
/// memory, another section will not be Any direct references between the two
/// sections would be errors For example, it would be an error if code in one
/// section called a function defined in the other section
///
/// The NOCROSSREFS command takes a list of output section names. If ld detects
/// any cross references between the sections, it reports an error and returns a
/// non-zero exit status. Note that the NOCROSSREFS command uses output section
/// names, not input section names.

struct DLL_A_EXPORT NoCrossRefs : public ScriptCommand {
  explicit NoCrossRefs(eld::NoCrossRefsCmd *NoCrossRefs);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_NoCrossRefs != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~NoCrossRefs() {}

public:
  eld::NoCrossRefsCmd *m_NoCrossRefs = nullptr;
};

/// \class OUTPUT
/// \brief Handle OUTPUT Command
///
/// OUTPUT(filename)
/// The OUTPUT command names the output file. Using OUTPUT(filename) in the
/// linker script is exactly like using ‘-o filename’ on the command line
/// (see Command Line Options). If both are used, the command-line option takes
/// precedence. You can use the OUTPUT command to define a default name for the
/// output file other than the usual default of a.out.
struct DLL_A_EXPORT Output : public ScriptCommand {
  explicit Output(eld::OutputCmd *Output);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Output != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Output() {}

public:
  eld::OutputCmd *m_Output = nullptr;
};

/// \class OUTPUT_ARCH
/// \brief Handle OUTPUT_ARCH Command
/// OUTPUT_ARCH ( name )
/// Specify a particular output machine architecture.

struct DLL_A_EXPORT OutputArch : public ScriptCommand {
  explicit OutputArch(eld::OutputArchCmd *OutputArch);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_OutputArch != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~OutputArch() {}

public:
  eld::OutputArchCmd *m_OutputArch = nullptr;
};

/// \class OUTPUT_FORMAT
/// \brief Handle OUTPUT_FORMAT Command
/// OUTPUT_FORMAT ( name )
/// Specify output format.
struct DLL_A_EXPORT OutputFormat : public ScriptCommand {
  explicit OutputFormat(eld::OutputFormatCmd *OutputFormat);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_OutputFormat != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~OutputFormat() {}

public:
  eld::OutputFormatCmd *m_OutputFormat = nullptr;
};

/// \class OUTPUT_SECTION
/// \brief Handle OutputSection Command
/// The full description of an output section looked like this:
///      section [address] [(type)] :
///        [AT(lma)]
///        [ALIGN(section_align)]
///        [SUBALIGN(subsection_align)]
///        [constraint]
///        {
///          output-section-command
///          output-section-command
///          ...
///        } [>region] [AT>lma_region] [:phdr :phdr ...] [=fillexp]

struct DLL_A_EXPORT OutputSectionSpec : public ScriptCommand {
  explicit OutputSectionSpec(eld::OutputSectDesc *OSD);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_OutputSectionSpec != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  bool hasMoreCommands() const override { return true; }

  /// \returns all commands inside SECTIONS command.
  std::vector<plugin::Script::ScriptCommand *> getCommands() const override;

  /// Destructor
  virtual ~OutputSectionSpec();

private:
  /// Populate the Output Section spec contents.
  void getOutputSectionSpecCommands();

public:
  eld::OutputSectDesc *m_OutputSectionSpec = nullptr;
  std::vector<plugin::Script::ScriptCommand *> m_OutputSectionSpecCommands;
};

/// \class PLUGIN
/// \brief Handle all Linker PLUGIN Commands
/// LINKER_PLUGIN
/// PLUGIN_CONTROL_FILESZ
/// PLUGIN_CONTROL_MEMSZ
/// PLUGIN_ITER_SECTIONS
/// PLUGIN_OUTPUT_SECTION_ITER
/// PLUGIN_SECTION_MATCHER
/// Handle all Linker plugins.
struct DLL_A_EXPORT Plugin : public ScriptCommand {
  explicit Plugin(eld::PluginCmd *P);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Plugin != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Plugin() {}

public:
  eld::PluginCmd *m_Plugin = nullptr;
};

/// \class SEARCH_DIR
/// \brief Handle SEARCH_DIR Command
///
/// SEARCH_DIR(path)
/// The SEARCH_DIR command adds path to the list of paths where ld looks for
/// archive libraries. Using SEARCH_DIR(path) is exactly like using ‘-L
/// path’ on the command line (see Command-line Options). If both are
/// used, then the linker will search both paths. Paths specified using the
/// command-line option are searched first.

struct DLL_A_EXPORT SearchDir : public ScriptCommand {
  explicit SearchDir(eld::SearchDirCmd *SearchDir);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_SearchDir != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~SearchDir() {}

public:
  eld::SearchDirCmd *m_SearchDir = nullptr;
};

/// \class SECTIONS Command
/// \brief Handle SECTIONS Command
///
/// The SECTIONS command tells the linker how to map input sections into output
/// sections, and how to place the output sections in memory.
///
/// The format of the SECTIONS command is:
///
///      SECTIONS
///      {
///        sections-command
///        sections-command
///        ...
///      }
/// Each sections-command may of be one of the following:
///
/// an ENTRY command (see Entry command)
/// a symbol assignment (see Assignments)
/// an output section description

struct DLL_A_EXPORT Sections : public ScriptCommand {
  explicit Sections(eld::SectionsCmd *SectionsCmd);

  /// \returns true when used in a boolean expression.
  operator bool() const { return m_Sections != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

  /// Destructor
  virtual ~Sections();

  /// \returns if the Command has additional commands.
  bool hasMoreCommands() const override { return true; }

  /// \returns all commands inside SECTIONS command.
  std::vector<plugin::Script::ScriptCommand *> getCommands() const override;

private:
  void getSectionCmdContents();

public:
  eld::SectionsCmd *m_Sections = nullptr;
  std::vector<plugin::Script::ScriptCommand *> m_SectionCommands;
};

/// OutputSectionData represents commands that are used to explicitly insert
/// bytes of data in an output section. These commands include BYTE, SHORT,
/// LONG, QUAD and SQUAD.
struct DLL_A_EXPORT OutputSectionData : public ScriptCommand {
  explicit OutputSectionData(eld::OutputSectData *OSD);

  /// \returns true if objects contains a command.
  operator bool() const { return m_OutputSectData != nullptr; }

  /// \returns the internal ScriptCommand object.
  eld::ScriptCommand *getCommand() const override;

private:
  eld::OutputSectData *m_OutputSectData = nullptr;
};
} // namespace Script

/// \class LinkerScript
/// \brief Handle Linker Script and provide lookup of PHDRS, OutputSections and
/// Rules
struct DLL_A_EXPORT LinkerScript {

  /// Constructor
  explicit LinkerScript(eld::LinkerScript *LinkerScript);

  /// Destructor
  virtual ~LinkerScript();

  /// \returns true/false - Allow object to be used in a condition.
  operator bool() const { return m_LinkerScript != nullptr; }

  /// Populate a internal vector of linker script commands
  void getLinkerScriptCommands();

  /// \returns Linker script commands from the linker script.
  std::vector<plugin::Script::ScriptCommand *> &getCommands();

  /// \returns true if the linkerscript has rules; returns false otherwise
  /// \note The rules in a linkerscript define how the linker should arrange
  /// sections of object files into the final executable or binary.
  bool linkerScriptHasRules();

  /// Get the linker script object
  eld::LinkerScript *getLinkerScript() const { return m_LinkerScript; }

  /// Return true if the script defines the SECTIONS command.
  bool hasSectionsCommand() const;

  /// Return the linker script hash.
  std::string getHash() const;

private:
  eld::LinkerScript *m_LinkerScript = nullptr;
  std::vector<plugin::Script::ScriptCommand *> m_ScriptCommands;
};

} // namespace eld::plugin

namespace std {
template <> struct hash<eld::plugin::LinkerScript> {
  std::size_t operator()(const eld::plugin::LinkerScript &L) const {
    return reinterpret_cast<std::size_t>(L.getLinkerScript());
  }
};
} // namespace std

#endif
