//===- GnuLinkerOptions.td ------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "llvm/Option/OptParser.td"

//===----------------------------------------------------------------------===//
/// Utility Functions
//===----------------------------------------------------------------------===//
// Single and multiple dash options combined
multiclass smDash<string opt1, string opt2, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(opt1)>;
  // Compatibility aliases
  def opt2_dashdash : Separate<["--"], opt2>, Alias<!cast<Option>(opt1)>;
  def opt2_dashdash_eq : Joined<["--"], opt2 #"=">, Alias<!cast<Option>(opt1)>;
}

// Single and multiple dash options combined
multiclass smDashWithOpt<string opt1, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
  // Compatibility aliases
  def opt1_dashdash : Separate<["--"], opt1>, Alias<!cast<Option>(realopt)>;
  def opt1_dashdash_eq : Joined<["--"], opt1 #"=">,
                         Alias<!cast<Option>(realopt)>;
}

// Single and multiple dash options combined
// -opt1 <x>
// -opt1<x>
// --opt2 x
// --opt2=x
// -opt2=x
// -opt2 <x>
multiclass
    smDashTwoWithOpt<string opt1, string opt2, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
  def opt2_joined_dash : Separate<["-"], opt2>, Alias<!cast<Option>(realopt)>;
  def opt1_joined : Joined<["-"], opt1>, Alias<!cast<Option>(realopt)>;
  // Compatibility aliases
  def opt1_dashdash : Separate<["--"], opt1>, Alias<!cast<Option>(realopt)>;
  def opt2_dashdash : Separate<["--"], opt2>,
                      HelpText<help>,
                      Alias<!cast<Option>(realopt)>;
  def opt2_dashdash_eq : Joined<["--"], opt2 #"=">,
                         Alias<!cast<Option>(realopt)>;
  def opt2_joined_dash_eq : Joined<["-"], opt2 #"=">,
                            Alias<!cast<Option>(realopt)>;
}

// Support -<option>,-<option>=
multiclass dashEq<string opt1, string opt2, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  // Compatibility aliases
  def opt2_eq : Joined<["-"], opt2 #"=">, Alias<!cast<Option>(opt1)>;
}

// Support --<option>,--<option>=
multiclass mDashEq<string opt1, string realopt, string help> {
  // Option
  def "" : Separate<["--"], opt1>, HelpText<help>;
  // Compatibility aliases
  def opt2_eq : Joined<["--"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
}

// --<option>,--<option>=
multiclass EEq<string name, string help> {
  def NAME: Separate<["--"], name>;
  def NAME # _eq: Joined<["--"], name # "=">, Alias<!cast<Separate>(NAME)>,
    HelpText<help>;
}

// Support -<option>,-<option>=
multiclass
    dashEqWithOpt<string opt1, string opt2, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  // Compatibility aliases
  def opt2_eq : Joined<["-"], opt2 #"=">, Alias<!cast<Option>(realopt)>;
}

// Support -<option></b><value>,-<option><value>,-<option>=
multiclass
    dashEqWithJoinedOpt<string opt1, string opt2, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  // Compatibility aliases
  def opt1_joined : Joined<["-"], opt1>, Alias<!cast<Option>(realopt)>;
  def opt2_eq : Joined<["-"], opt2 #"=">, Alias<!cast<Option>(realopt)>;
}

// Support -<option>=,--<option>=
multiclass smDashOnlyEq<string opt1, string realopt, string help> {
  def "" : Joined<["-"], opt1 #"=">, HelpText<help>;
  def opt1_eq : Joined<["--"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
}

// Support -<option>=,--<option>= aliases
multiclass smDashOnlyEqAlias<string opt1, string realopt, string help> {
  def "" : Joined<["-"], opt1#"=">,
           Alias<!cast<Option>(realopt)>,
           HelpText<help>;
  def opt1_eq : Joined<["--"], opt1#"=">,
                Alias<!cast<Option>(realopt)>,
                HelpText<help>;
}

// The definitions below are copied from lld.

// For options whose names are multiple letters, either one dash or
// two can precede the option name except those that start with 'o'.
class F<string name> : Flag<["--", "-"], name>;
class J<string name> : Joined<["--", "-"], name>;

// Convenience classes for long options which only accept two dashes. For lld
// specific or newer long options, we prefer two dashes to avoid collision with
// short options. For many others, we have to accept both forms to be compatible
// with GNU ld.
class FF<string name> : Flag<["--"], name>;
class JJ<string name> : Joined<["--"], name>;

//===----------------------------------------------------------------------===//
/// General Options
//===----------------------------------------------------------------------===//
def grp_general : OptionGroup<"opts">, HelpText<"GENERAL OPTIONS">;
defm output_file : smDashTwoWithOpt<"o", "output", "output_file",
                                    "Path to file to write output">,
                   MetaVarName<"<outputfile>">,
                   Group<grp_general>;
defm sysroot : smDash<"sysroot", "sysroot", "Set the system root">,
              MetaVarName<"<sysroot-path>">,
               Group<grp_general>;
def version : Flag<["--"], "version">,
              HelpText<"Print the Linker version">,
              Group<grp_general>;
def repository_version : Flag<["--"], "repository-version">,
                         HelpText<"Print the Linker Repository version">,
                         Group<grp_general>;
def build_id
    : Flag<["--"], "build-id">,
      HelpText<"Request creation of \".note.gnu.build-id\" ELF note section">,
      Group<grp_general>;
defm build_id_val
    : smDashOnlyEq<
          "build-id", "build_id_val",
          "Request creation of \".note.gnu.build-id\" ELF note section">,
           MetaVarName<"fast/md5/sha1/tree/uuid/0x<hexstring>/none">,
      Group<grp_general>;

//===----------------------------------------------------------------------===//
/// LLVM and Target options
//===----------------------------------------------------------------------===//
def grp_llvmtarget : OptionGroup<"opts">, HelpText<"LLVM and Target Options">;
def mtriple : Separate<["-"], "mtriple">,
              MetaVarName<"<triple>">,
              HelpText<"Target triple to link for">,
              Group<grp_llvmtarget>;
def mtriple_alias : Joined<["-"], "mtriple=">,
                    MetaVarName<"<triple>">,
                    HelpText<"Target triple to link for">,
                    Alias<mtriple>;
def march : Separate<["-"], "march">,
            MetaVarName<"<march>">,
            HelpText<"Target architecture">,
            Group<grp_llvmtarget>;
def march_alias : Joined<["-"], "march=">,
                  MetaVarName<"<march>">,
                  HelpText<"Target architecture">,
                  Alias<march>;
def mabi : Separate<["-"], "mabi">,
           MetaVarName<"<mabi>">,
           HelpText<"Target ABI">,
           Group<grp_llvmtarget>;
def mabi_alias : Joined<["-"], "mabi=">,
                 MetaVarName<"<mabi>">,
                 HelpText<"Target ABI">,
                 Alias<mabi>;
def mcpu : Separate<["-"], "mcpu">,
           MetaVarName<"<mcpu>">,
           HelpText<"Target CPU">,
           Group<grp_llvmtarget>;
def mcpu_alias : Joined<["-"], "mcpu=">, MetaVarName<"<mcpu>">, Alias<mcpu>;
def mllvm : Separate<["-"], "mllvm">,
            MetaVarName<"<option>">,
            HelpText<"Options to pass to LLVM">,
            Group<grp_llvmtarget>;
def emulation : JoinedOrSeparate<["-"], "m">,
                MetaVarName<"<emulation>">,
                HelpText<"Select target emulation">,
                Group<grp_llvmtarget>;

//===----------------------------------------------------------------------===//
/// Script Options
//===----------------------------------------------------------------------===//
def grp_scriptopts : OptionGroup<"opts">, HelpText<"SCRIPT OPTIONS">;
defm T : smDashTwoWithOpt<
             "T", "script", "T",
             "Use the given linker script in place of the default script.">,
         MetaVarName<"<linkerscriptfile>">,
         Group<grp_scriptopts>;
defm version_script
    : smDashWithOpt<"version-script", "version_script",
                    "Use the linker script as a version script.">,
      MetaVarName<"<linkersciptfile>">,
      Group<grp_scriptopts>;
defm default_script
    : smDashWithOpt<"default-script", "default_script",
                    "Use the linker script as the default linker script.">,
      MetaVarName<"<pluginfile>">,
      Group<grp_scriptopts>;
defm Ttext
    : smDash<"Ttext", "Ttext", "Specify an address for the .text section">,
      MetaVarName<"<address>">,
      Group<grp_scriptopts>;
defm Ttext_segment
    : dashEqWithOpt<"Ttext-segment", "Ttext-segment", "Ttext_segment",
                    "Specify an address for the .text-segment segment">,
      MetaVarName<"<address>">,
      Group<grp_scriptopts>;
defm Tdata
    : smDash<"Tdata", "Tdata", "Specify an address for the .data section">,
      MetaVarName<"<address>">,
      Group<grp_scriptopts>;
defm Tbss : smDash<"Tbss", "Tbss", "Specify an address for the .bss section">,
            MetaVarName<"<address>">,
            Group<grp_scriptopts>;
defm dynamic_list
    : smDashWithOpt<"dynamic-list", "dynamic_list",
                    "Specify a list of symbols if present will be exported">,
      MetaVarName<"<list-of-symbols>">,
      Group<grp_scriptopts>;
defm extern_list
    : smDashWithOpt<
          "extern-list", "extern_list",
          "Specify a list of symbols that exists as external dependencies">,
      MetaVarName<"<list-of-symbols>">,
      Group<grp_scriptopts>;
defm map_section
    : mDashEq<"map-section", "map_section",
              "Specify a input section that maps to a output section">,
      MetaVarName<"<section>">,
      Group<grp_scriptopts>;
defm section_start
    : smDashWithOpt<
          "section-start", "section_start",
          "Specify a virtual output section address for a specified section">,
      MetaVarName<"<address>">,
      Group<grp_scriptopts>;
defm orphan_handling
    : smDashWithOpt<
          "orphan-handling", "orphan_handling",
          "Specify how to handle orphan sections. Options available are place,"
          "error, warn">,
      MetaVarName<"<mode>">,
      Group<grp_scriptopts>;
defm exclude_libs : mDashEq<"exclude-libs", "exclude_libs",
                            "Specifies a list of archive libraries from which "
                            "symbols should not be automatically exported">,
                    MetaVarName<"<libs>">,
                    Group<grp_scriptopts>;
def unique_output_sections
    : Flag<["-", "--"], "unique-output-sections">,
      HelpText<"Place each input section in a unique output section">,
      Group<grp_scriptopts>;
def global_merge_non_alloc_strings
    : Flag<["--"], "global-merge-non-alloc-strings">,
      HelpText<"merge non-alloc strings across output sections">,
      Group<grp_scriptopts>;
def disable_overlap_checks
    : Flag<["--"], "no-check-sections">,
      HelpText<"Do not check section addresses for overlaps">,
      Group<grp_scriptopts>;
def enable_overlap_checks
    : Flag<["--"], "check-sections">,
      HelpText<"Check section addresses for overlaps(default)">,
      Group<grp_scriptopts>;
def print_memory_usage
    : Flag<["--"], "print-memory-usage">,
      HelpText<"Print memory usage when MEMORY linker script directive is used">,
      Group<grp_scriptopts>;

//===----------------------------------------------------------------------===//
/// Output Kinds
//===----------------------------------------------------------------------===//
def grp_kind : OptionGroup<"outs">, HelpText<"OUTPUT KIND">;
def relocatable : Flag<["-"], "r">,
                  HelpText<"Create relocatable object file">,
                  Group<grp_kind>;
def static_link : Flag<["-"], "static">,
             HelpText<"Create static executable">,
             Group<grp_kind>;
def dynamic : Flag<["-"], "dynamic">,
              HelpText<"Create dynamic executable (default)">,
              Group<grp_kind>;
def shared : Flag<["-"], "shared">,
             HelpText<"Create dynamic library">,
             Group<grp_kind>;
def pie : Flag<["-"], "pie">,
          HelpText<"Create PIE executable">,
          Group<grp_kind>;
def no_pie : Flag<["-"], "no-pie">,
          HelpText<"Do not create a PIE executable">,
          Group<grp_kind>;
def picexec : Flag<["--"], "pic-executable">,
              Group<grp_kind>,
              HelpText<"Create PIE executable">,
              Alias<pie>;
// output kinds - compatibility aliases
def Bstatic : Flag<["-"], "Bstatic">,
              Group<grp_kind>,
              HelpText<"Create static executable">,
              Alias<static_link>;
def dn : Flag<["-"], "dn">,
         Group<grp_kind>,
         HelpText<"Create static executable">,
         Alias<static_link>;
def non_shared : Flag<["-"], "non_shared">,
                 Group<grp_kind>,
                 HelpText<"Create static executable">,
                 Alias<static_link>;
def Bshareable : Flag<["-"], "Bshareable">,
                 HelpText<"Create dynamic library">,
                 Alias<shared>;
def Bdynamic : Flag<["-"], "Bdynamic">,
               Group<grp_kind>,
               HelpText<"Link against dynamic library">;
def BdynamicAlias1 : Flag<["-"], "dy">,
                     Group<grp_kind>,
                     HelpText<"Link against dynamic library">,
                     Alias<Bdynamic>;
def BdynamicAlias2 : Flag<["-"], "call_shared">,
                     Group<grp_kind>,
                     HelpText<"Link against dynamic library">,
                     Alias<Bdynamic>;

//===----------------------------------------------------------------------===//
/// Executable Options
//===----------------------------------------------------------------------===//
def grp_main : OptionGroup<"opts">, HelpText<"EXECUTABLE OPTIONS">;
def L : JoinedOrSeparate<["-"], "L">,
        MetaVarName<"<path>">,
        HelpText<"Path to search for libraries or linker scripts">,
        Group<grp_main>;
def l : JoinedOrSeparate<["-"], "l">,
        MetaVarName<"<libName>">,
        HelpText<"Root name of library to use">,
        Group<grp_main>;
def namespec : Joined<["--"], "library=">,
               MetaVarName<"<namespec>">,
               HelpText<"library to use">,
               Group<grp_main>;
defm library_path
    : mDashEq<"library-path", "library_path",
              "Path to search for libraries or linker scripts">,
      MetaVarName<"<path>">,
      Group<grp_main>;
defm Y : smDash<"Y", "Y", "Add path to the default library search path">,
         MetaVarName<"<path>">,
         Group<grp_main>;
def noinhibit_exec : Flag<["--"], "noinhibit-exec">,
                     HelpText<"Retain the executable output file whenever"
                              " it is still usable">,
                     Group<grp_main>;
defm entrypoint : smDashTwoWithOpt<"e", "entry", "entrypoint",
                                   "Name of entry point symbol">,
                  MetaVarName<"<symbolname>">,
                  Group<grp_main>;

defm image_base : EEq<"image-base", "Set the base address">,
                  MetaVarName<"<address>">,
                  Group<grp_main>;

defm init : smDash<"init", "init", "Specify an initializer function">,
            MetaVarName<"<symbol>">,
            Group<grp_main>;
defm fini : smDash<"fini", "fini", "Specify a finalizer function">,
            MetaVarName<"<symbol>">,
            Group<grp_main>;
def whole_archive : Flag<["-", "--"], "whole-archive">,
                    HelpText<"Force load of all members in a static library">,
                    Group<grp_main>;
def no_whole_archive
    : Flag<["-", "--"], "no-whole-archive">,
      HelpText<"Restores the default behavior of loading archive members">,
      Group<grp_main>;
def nostdlib : Flag<["-"], "nostdlib">,
               HelpText<"Disable default search path for libraries">,
               Group<grp_main>;
def emit_relocs : Flag<["-", "--"], "emit-relocs">,
                  HelpText<"Make Emit relocs behave just like GNU.">,
                  Group<grp_main>;
def omagic
    : Flag<["--"], "omagic">,
      HelpText<"Set the text and data sections to be readable and writable."
               " Also, do not page-align the data segment, and"
               " disable linking against shared libraries.">,
      Group<grp_main>;
def no_omagic
    : Flag<["--"], "no-omagic">,
      HelpText<"This option negates most of the effects of the -N option."
               "Disable linking with shared libraries">,
      Group<grp_main>;
def omagic_alias
    : Flag<["-"], "N">,
      HelpText<"Set the text and data sections to be readable and writable."
               " Also, do not page-align the data segment, and"
               " disable linking against shared libraries.">,
      Alias<omagic>,
      Group<grp_main>;

//===----------------------------------------------------------------------===//
/// Dynamic Library/Executable Options
//===----------------------------------------------------------------------===//
def grp_dynlibexec : OptionGroup<"opts">,
                     HelpText<"DYNAMIC LIBRARY/EXECUTABLE OPTIONS">;
defm dynamic_linker : smDashWithOpt<"dynamic-linker", "dynamic_linker",
                                    "Set the path to the dynamic linker">,
                      MetaVarName<"<path>">,
                      Group<grp_dynlibexec>;
defm rpath : smDash<"rpath", "rpath",
                    "Add a path to the runtime library search path">,
             MetaVarName<"<path>">,
             Group<grp_dynlibexec>;
defm rpath_link
    : dashEqWithOpt<"rpath-link", "rpath-link", "rpath_link",
                    "Specifies the path to search">,
      MetaVarName<"<path>">,
      Group<grp_dynlibexec>;
def export_dynamic : Flag<["-", "--"], "export-dynamic">,
                     HelpText<"Add all symbols to the dynamic symbol table"
                              " when creating executables">,
                     Group<grp_dynlibexec>;
def force_dynamic : Flag<["-", "--"], "force-dynamic">,
                    HelpText<"Build executable as a force dynamic executable">,
                    Group<grp_dynlibexec>;
def alias_export_dynamic : Flag<["-"], "E">,
                           HelpText<"Add all symbols to the dynamic symbol "
                                    "table when creating executables">,
                           Group<grp_dynlibexec>,
                           Alias<export_dynamic>;
def no_export_dynamic : Flag<["--"], "no-export-dynamic">,
                        Group<grp_dynlibexec>;
defm export_dynamic_symbol
    : smDashWithOpt<"export-dynamic-symbol", "export_dynamic_symbol",
                    "Export specified symbol to dynamic symbol table">,
      MetaVarName<"<symbol>">,
      Group<grp_dynlibexec>;
def no_dynamic_linker : Flag<["-", "--"], "no-dynamic-linker">,
                     HelpText<"The program does not need a dynamic linker"
                              " when creating static PIE executables">,
                     Group<grp_dynlibexec>;

//===----------------------------------------------------------------------===//
/// Dynamic Library Options
//===----------------------------------------------------------------------===//
def grp_dynlib : OptionGroup<"opts">, HelpText<"DYNAMIC LIBRARY OPTIONS">;
def soname : Joined<["-", "--"], "soname=">,
             HelpText<"Set the internal DT_SONAME field to the specified name">,
             MetaVarName<"<name>">,
             Group<grp_dynlib>;
def soname_separate
    : Separate<["-", "--"], "soname">,
      HelpText<"Set the internal DT_SONAME field to the specified name">,
      Alias<soname>,
      MetaVarName<"<name>">,
      Group<grp_dynlib>;
def soname_h
    : Separate<["-"], "h">,
      HelpText<"Set the internal DT_SONAME field to the specified name">,
      Alias<soname>,
      MetaVarName<"<name>">,
      Group<grp_dynlib>;
def dash_g
    : Flag<["-"], "g">,
      HelpText<
          "Enable debug output when building shared libraries or executables">,
      Group<grp_dynlib>;
defm hash_size : mDashEq<"hash-size", "hash_size",
                         "Specify a hash size when creating the hash sections "
                         "for the dynamic loader">,
                 MetaVarName<"<size>">,
                 Group<grp_dynlib>;
defm hash_style : smDashWithOpt<"hash-style", "hash_style",
                                "Specify a hash style when creating the hash "
                                "sections for the dynamic loader">,
                  MetaVarName<"<hashstyle>">,
                  Group<grp_dynlib>;
def warn_shared_textrel : Flag<["--"], "warn-shared-textrel">,
                          HelpText<"Warn if the linker adds a DT_TEXTREL">,
                          Group<grp_dynlib>;
def no_warn_shared_textrel
    : Flag<["--"], "no-warn-shared-textrel">,
      HelpText<"Don't Warn if the linker adds a DT_TEXTREL">,
      Group<grp_dynlib>;
def Bsymbolic : Flag<["-"], "Bsymbolic">,
                HelpText<"Bind references to global symbols to the definition "
                         "within the shared library">,
                Group<grp_dynlib>;
def Bsymbolic_functions : Flag<["-"], "Bsymbolic-functions">,
                          HelpText<"Bind references to global functions to the "
                                   "definition within the shared library">,
                          Group<grp_dynlib>;
def Bgroup
    : Flag<["-"], "Bgroup">,
      HelpText<"Enable runtime linker to handle lookups in this object and its "
               "dependencies to be performed only inside this group">,
      Group<grp_dynlib>;
def fPIC : Flag<["-"], "fPIC">, HelpText<"Enable PIC mode">, Group<grp_dynlib>;

//===----------------------------------------------------------------------===//
/// Resolver Options
//===----------------------------------------------------------------------===//
def grp_resolveropt : OptionGroup<"opts">,
                      HelpText<"SYMBOL RESOLUTION OPTIONS">;
def u : JoinedOrSeparate<["-"], "u">,
        MetaVarName<"<symbol>">,
        Group<grp_resolveropt>,
        HelpText<"Force symbol to be entered in the output file"
                 " as an undefined symbol">;
def u_alias : Joined<["--"], "undefined=">,
              HelpText<"Force symbol to be entered in the output file as an "
                       "undefined symbol">,
              MetaVarName<"<symbol>">,
              Group<grp_resolveropt>,
              Alias<u>;
def start_group : Flag<["-", "--"], "start-group">,
                  HelpText<"Start a group">,
                  Group<grp_resolveropt>;
def alias_start_group : Flag<["-"], "(">,
                        HelpText<"Start a group">,
                        Group<grp_resolveropt>,
                        Alias<start_group>;
def end_group : Flag<["-", "--"], "end-group">,
                HelpText<"End a group">,
                Group<grp_resolveropt>;
def alias_end_group : Flag<["-"], ")">,
                      HelpText<"End a group">,
                      Group<grp_resolveropt>,
                      Alias<end_group>;
def as_needed : Flag<["--"], "as-needed">,
                HelpText<"This option affects ELF DT_NEEDED tags for "
                         "dynamic libraries mentioned on the command line">,
                Group<grp_resolveropt>;
def no_as_needed : Flag<["--"], "no-as-needed">,
                   HelpText<"This option restores the default behavior"
                            " of adding DT_NEEDED entries">,
                   Group<grp_resolveropt>;
def push_state : Flag<["--"], "push-state">,
                 HelpText<"Save current input handling flags (as-needed, whole-archive, etc)">,
                 Group<grp_resolveropt>;
def pop_state : Flag<["--"], "pop-state">,
                HelpText<"Restore previously saved input handling flags">,
                Group<grp_resolveropt>;
def allow_shlib_undefs : Flag<["-", "--"], "allow-shlib-undefined">,
                         HelpText<"Allow undefined symbols from dynamic"
                                  " library when creating executables">,
                         Group<grp_resolveropt>;
def use_shlib_undefs
    : Flag<["--"], "use-shlib-undefines">,
      HelpText<"Resolve undefined symbols from dynamic libraries">,
      Group<grp_resolveropt>;
def allow_multiple_definition : Flag<["--"], "allow-multiple-definition">,
                                HelpText<"Allow multiple definitions">,
                                Group<grp_resolveropt>;
defm defsym : smDash<"defsym", "defsym",
                     "Create a global symbol in the output file "
                     "containing the absolute address given by expression">,
              MetaVarName<"symbol=<expression>">,
              Group<grp_resolveropt>;
def no_undefined
    : Flag<["-", "--"], "no-undefined">,
      HelpText<"Report unresolved symbol references from regular object files">,
      Group<grp_resolveropt>;
def warn_once : Flag<["--"], "warn-once">,
                HelpText<"Warn only once for every undefined reference">,
                Group<grp_resolveropt>;
defm unresolved_symbols
    : mDashEq<
          "unresolved-symbols", "unresolved_symbols",
          "Determine how to handle unresolved symbols, Options available are "
          "ignore-all,"
          "report-all(Default), ignore-in-object-files, ignore-in-shared-libs">,
      MetaVarName<"<option>">,
      Group<grp_scriptopts>;
defm sort_section : EEq<"sort-section",
                       "Sort sections. Options available are name , alignment">,
                       MetaVarName<"<sort_section>">,
                        Group<grp_scriptopts>;

//===----------------------------------------------------------------------===//
/// Symbol options
//===----------------------------------------------------------------------===//
def grp_symbolopts : OptionGroup<"opts">, HelpText<"SYMBOL OPTIONS">;
defm demangle_style
    : mDashEq<"demangle-style", "demangle_style",
              "Specify whether the linker should demangle symbols when "
              "emitting errors or emitting Map files">,
      Group<grp_symbolopts>;
def demangle : Flag<["--"], "demangle">,
               HelpText<"Demangle C++ symbols">,
               Group<grp_symbolopts>;
def discard_locals : Flag<["--"], "discard-locals">,
                     HelpText<"Discard all local symbols">,
                     Group<grp_symbolopts>;
def alias_discard_loc : Flag<["-"], "X">,
                        HelpText<"Discard all local symbols">,
                        Group<grp_symbolopts>,
                        Alias<discard_locals>;
def discard_all : Flag<["--"], "discard-all">,
                  HelpText<"Discard all symbols">,
                  Group<grp_symbolopts>;
def x : Flag<["-"], "x">,
        HelpText<"Discard all symbols">,
        Group<grp_symbolopts>,
        Alias<discard_all>;
def strip_all : Flag<["--"], "strip-all">,
                HelpText<"Omit all symbol informations from output">,
                Group<grp_symbolopts>;
def s : Flag<["-"], "s">,
        HelpText<"Omit all symbol informations from output">,
        Group<grp_symbolopts>,
        Alias<strip_all>;
def no_demangle : Flag<["--"], "no-demangle">,
                  HelpText<"Dont demangle C++ symbols">,
                  Group<grp_symbolopts>;
def strip_debug : Flag<["--"], "strip-debug">,
                  HelpText<"Omit all debug information from output">,
                  Group<grp_symbolopts>;
def S : Flag<["-"], "S">,
        HelpText<"Omit all debug information from output">,
        Group<grp_symbolopts>,
        Alias<strip_debug>;
defm wrap : smDash<"wrap", "wrap", "Specify symbol to be wrapped to">,
            MetaVarName<"<symbol>">,
            Group<grp_symbolopts>;
defm portable : smDash<"portable", "portable",
                       "Specify symbol to be portable wrapped to">,
                MetaVarName<"<symbol>">,
                Group<grp_symbolopts>;
def d : Flag<["-"], "d">,
        HelpText<"Assign space to common symbols">,
        Group<grp_symbolopts>;
def dc : Flag<["-"], "dc">,
         HelpText<"Assign space to common symbols">,
         Alias<d>;
def dp : Flag<["-"], "dp">,
         HelpText<"Assign space to common symbols">,
         Alias<d>;
def sort_common : Flag<["--"], "sort-common">,
         HelpText<"sort common symbols by alignment">,
         Group<grp_symbolopts>;
defm sort_common_val : smDashOnlyEq<"sort-common", "sort_common_val",
                       "Sort common symbols by ascdending/descending order of alignment">,
                       MetaVarName<"<option>">,
                        Group<grp_symbolopts>;

//===----------------------------------------------------------------------===//
/// Diagnostic options
//===----------------------------------------------------------------------===//
def grp_diagopts : OptionGroup<"opts">, HelpText<"DIAGNOSITC OPTIONS">;
defm trace
    : smDash<
          "trace", "trace",
          "Allow tracing of\n"
          "\t\t\t --trace=linker-script : trace linker script\n"
          "\t\t\t --trace=files : trace input files\n"
          "\t\t\t --trace=reloc=<pattern> : trace relocations\n"
          "\t\t\t --trace=symbol=<symbol-name> : trace symbol\n"
          "\t\t\t --trace=all-symbols : trace all symbols\n"
          "\t\t\t --trace=LTO : trace LTO\n"
          "\t\t\t --trace=garbage-collection : trace linker garbage "
          "collection\n"
          "\t\t\t --trace=plugin : trace plugin\n"
          "\t\t\t --trace=threads : trace threads\n"
          "\t\t\t --trace=assignments : trace symbol assignments\n"
          "\t\t\t --trace=command-line : trace header info\n"
          "\t\t\t --trace=live-edges : trace reachable sections when garbage "
          "collection is enabled\n"
          "\t\t\t --trace=merge-strings : trace linker string optimization\n"
          "\t\t\t --trace=trampolines : trace trampolines\n"
          "\t\t\t --trace=wrap-symbols : trace symbol wrap options\n"
          "\t\t\t --trace=symdef : trace symbol resolution from symdef files\n"
          "\t\t\t --trace=dynamic-linking : trace dynamic linking">,
      MetaVarName<"<trace-type>">,
      Group<grp_diagopts>;
defm trace_symbol : smDashTwoWithOpt<"y", "trace-symbol", "trace_symbol",
                                  "Trace a particular symbol">,
                    MetaVarName<"<symbol>">,
                    Group<grp_diagopts>;
def trace_lto : Flag<["--"], "trace-lto">,
                HelpText<"Trace stages of lto">,
                Group<grp_diagopts>;
def trace_linker_script : Flag<["--"], "trace-linker-script">,
                HelpText<"Trace stages of linker script processing">,
                Group<grp_diagopts>;
defm trace_section : smDashWithOpt<"trace-section", "trace_section",
                                   "Show metadata about a particular section">,
                     MetaVarName<"<section_name>">,
                     Group<grp_diagopts>;
defm trace_reloc : smDashWithOpt<"trace-reloc", "trace_reloc",
                                 "trace a particular relocation">,
                   MetaVarName<"<relocation>">,
                   Group<grp_diagopts>;
defm trace_merge_strings
    : smDashWithOpt<"trace-merge-strings", "trace_merge_strings",
          "Emit diagnostics describing how strings were merged. Options: "
          "all(default), allocatable_sections, <output section (regex)>">,
      MetaVarName<"<option>">,
      Group<grp_diagopts>;
defm verify_options : smDashWithOpt<"verify-options", "verify_options",
                                    "Verify certain internal linker "
                                    "computations - currently supports reloc">,
                      MetaVarName<"<option>">,
                      Group<grp_diagopts>;
def dash_t : Flag<["-"], "t">,
             HelpText<"Print all files processed by the linker">,
             Group<grp_diagopts>;
def cref : Flag<["-", "--"], "cref">,
           HelpText<"Print the references for a symbol or section">,
           Group<grp_diagopts>;
defm emit_timing_stats : mDashEq<"emit-timing-stats", "emit_timing_stats",
                                 "Emit time statistics of various linker "
                                 "operatons to the specified file">,
                         MetaVarName<"<filename>">,
                         Group<grp_diagopts>;

def emit_timing_stats_in_output
    : Flag<["-", "--"], "emit-timing-stats-in-output">,
      HelpText<"Insert link-time stats in section .note.qc.timing">,
      Group<grp_diagopts>;

defm time_region : mDashEq<"time-region", "time_region",
                   "Emit time statistics for specified region of linker operation ">,
                            MetaVarName<"<region>">,
                             Group<grp_diagopts>;

def print_timing_stats
    : Flag<["-", "--"], "print-timing-stats">,
      HelpText<"Print time statistics of various linker operatons to console">,
      Group<grp_diagopts>;
defm gc_cref : mDashEq<"gc-cref", "gc_cref",
                       "Print the references for a symbol or section when "
                       "garbage collection is enabled">,
              MetaVarName<"<symbol/section>">,
               Group<grp_diagopts>;
def verbose : Flag<["--"], "verbose">,
              HelpText<"Enable verbose output">,
              Group<grp_diagopts>;
defm verbose_level
    : smDashOnlyEq<"verbose", "verbose_level", "Enable verbose output">,
      MetaVarName<"<verbose-level>">,
      Group<grp_diagopts>;
def fatal_warnings : Flag<["--"], "fatal-warnings">,
                     HelpText<"Enable fatal warnings">,
                     Group<grp_diagopts>;
def no_fatal_warnings : Flag<["--"], "no-fatal-warnings">,
                        HelpText<"Disable fatal warnings">,
                        Group<grp_diagopts>;
def warn_common : Flag<["--"], "warn-common">,
                  HelpText<"Warn on common symbols">,
                  Group<grp_diagopts>;
def no_warn_mismatch
    : Flag<["--"], "no-warn-mismatch">,
      HelpText<"Do not Warn on incompatible files passed to the linker.">,
      Group<grp_diagopts>;
def warn_mismatch
    : Flag<["--"], "warn-mismatch">,
      HelpText<"Warn on incompatible files passed to the linker.">,
      Group<grp_diagopts>;
defm error_style
    : mDashEq<"error-style", "error_style", "Specify an error style, LLVM/GNU">,
      MetaVarName<"<style>">,
      Group<grp_diagopts>;
defm color : smDash<"color", "color", "Enable color output for diagnostics">,
             MetaVarName<"<on/off>">,
             Group<grp_diagopts>;
def opt_record_file : Flag<["-", "--"], "opt-record-file">,
                      HelpText<"Create diagnostic yaml file">,
                      Group<grp_diagopts>;
defm display_hotness
    : dashEq<"display_hotness", "display-hotness",
             "Display hotness information for optimization remarks">,
      Group<grp_diagopts>;
def progress_bar : Flag<["--"], "progress-bar">,
                   HelpText<"Show Progress Bar">,
                   Group<grp_diagopts>;
def summary : Flag<["--"], "summary">,
              HelpText<"Display linker run summary at the end">,
              Group<grp_diagopts>;
def fatal_internal_errors : Flag<["--"], "fatal-internal-errors">,
                     HelpText<"Enable fatal internal errors">,
                     Group<grp_diagopts>;
def no_fatal_internal_errors : Flag<["--"], "no-fatal-internal-errors">,
                     HelpText<"Disable fatal internal errors">,
                     Group<grp_diagopts>;
defm error_limit
    : mDashEq<"error-limit", "error_limit",
              "Maximum number of errors to emit before stopping (0 = no limit)">,
      MetaVarName<"<max-error-number>">,
      Group<grp_diagopts>;
defm warn_limit
    : mDashEq<"warn-limit", "warn_limit",
              "Maximum number of warnings to emit (0 = no limit)">,
      MetaVarName<"<maxwarnings>">,
      Group<grp_diagopts>;
def W : Joined<["-"], "W">,
        MetaVarName<"<warn-type>">,
        HelpText<"Print warnings which are OFF by default\n"
             "\t\t\t -Wall : print all warnings\n"
             "\t\t\t -W[no]linker-script : display warnings detected"
             " while parsing linker scripts\n"
             "\t\t\t -W[no]attribute-mix : display a warning if object files"
             " of RISC-V with different attributes are being linked\n"
             "\t\t\t -W[no-]archive-file : display warnings detected while "
             "reading archive files\n"
             "\t\t\t -W[no-]linker-script-memory : display warnings detected while "
             "processing linker script memory command\n"
             "\t\t\t -W[no-]bad-dot-assignments : display warnings for bad "
             "dot assignments\n"
             "\t\t\t -W[no-]error : treat warnings as errors\n"
             "\t\t\t -Wwhole-archive : display a warning when whole-archive "
             "is enabled for any archive file\n"
             "\t\t\t -W[no-]osabi : display a warning when linking objects "
             "with different values for OS/ABI\n"
            >,
        Group<grp_diagopts>;

//===----------------------------------------------------------------------===//
/// Optimization options
//===----------------------------------------------------------------------===//
def grp_optimizationopts : OptionGroup<"opts">,
                           HelpText<"OPTIMIZATION OPTIONS">;
def gc_sections : Flag<["-", "--"], "gc-sections">,
                  HelpText<"Enable garbage collection">,
                  Group<grp_optimizationopts>;
def no_gc_sections : Flag<["-", "--"], "no-gc-sections">,
                     HelpText<"Disable garbage collection">,
                     Group<grp_optimizationopts>;
def print_gc_sections : Flag<["-", "--"], "print-gc-sections">,
                        HelpText<"Print sections that are garbage collected">,
                        Group<grp_optimizationopts>;
def eh_frame_hdr
    : Flag<["--"], "eh-frame-hdr">,
      HelpText<"Create EH Frame Header section for faster exception handling">,
      Group<grp_optimizationopts>;
def no_merge_strings : Flag<["--"], "no-merge-strings">,
                       HelpText<"Disable String Merging">,
                       Group<grp_optimizationopts>;
def no_trampolines : Flag<["--"], "no-trampolines">,
                     HelpText<"Disable Trampolines">,
                     Group<grp_optimizationopts>;

//===----------------------------------------------------------------------===//
/// Extended Options
//===----------------------------------------------------------------------===//
def grp_extendedopts : OptionGroup<"opts">, HelpText<"Extended Options">;
defm dash_z
    : dashEqWithJoinedOpt<
          "z", "z", "dash_z",
          "Extended Options or Non standard options.\n\t\t\t Available options are:\n"
          "\t\t\t-z=combreloc : Combines multiple reloc sections and sorts them to make dynamic"
          "symbol lookup caching\n"
          "\t\t\t-z=now : Enables immediate binding\n"
          "\t\t\t-z=nocopyreloc : Disables Copy Relocation">,
      MetaVarName<"<extended-opts>">,
      Group<grp_extendedopts>;
def no_align_segments : Flag<["--"], "no-align-segments">,
                        HelpText<"Dont align segments to page boundaries">,
                        Group<grp_extendedopts>;
def emit_relocs_llvm : Flag<[ "-", "--" ], "emit-relocs-llvm">,
                       HelpText<"Emit relocations sections">,
                       Group<grp_extendedopts>;
def align_segments : Flag<["--"], "align-segments">,
                     HelpText<"Align segments to page boundaries">,
                     Group<grp_extendedopts>;
def no_verify : Flag<["--"], "no-verify">,
                HelpText<"Dont verify link output">,
                Group<grp_extendedopts>;
def disable_newdtags : Flag<["--"], "disable-new-dtags">,
                       HelpText<"Disable new dynamic tags">,
                       Group<grp_extendedopts>;
def enable_newdtags : Flag<["--"], "enable-new-dtags">,
                      HelpText<"Enable new dynamic tags">,
                      Group<grp_extendedopts>;
def rosegment
    : Flag<[ "-", "--" ], "rosegment">,
      HelpText<"Put read-only non-executable sections in their own segment">,
      Group<grp_extendedopts>;
defm copy_farcalls_from_file
    : smDashWithOpt<"copy-farcalls-from-file", "copy_farcalls_from_file",
                    "Copy far calls instead of using trampolines">,
                    MetaVarName<"<filename>">,
      Group<grp_extendedopts>;
defm no_reuse_trampolines_file
    : smDashWithOpt<"no-reuse-trampolines-file", "no_reuse_trampolines_file",
                    "Dont reuse trampolines for symbols specified in file">,
      MetaVarName<"<filename>">,
      Group<grp_extendedopts>;
def use_old_style_trampoline_name
    : Flag<["--"],"use-old-style-trampoline-name">,
      HelpText<"Use old style of naming trampolines">,
      Group<grp_extendedopts>;
def no_emit_relocs : Flag<[ "-", "--" ], "no-emit-relocs">,
                     HelpText<"Dont emit relocations in the output file, "
                              "applicable to --emit-relocs-llvm option too ">,
                     Group<grp_extendedopts>;
defm script_options : mDashEq<"script-options", "script_options",
                              "Specify a match type, match-gnu/match-llvm ">,
                      MetaVarName<"<matchtype>">,
                      Group<grp_extendedopts>;
def allow_bss_conversion
    : Flag<["--"], "allow-bss-conversion">,
      HelpText<"Dont produce an error when mixing NOBITS, and PROGBITS "
               "sections in the same segment">,
      Group<grp_extendedopts>;
defm reproduce : smDash<"reproduce", "reproduce",
                        "Write a tar file containing input files and command "
                        "line options to inspect and re-link">,
                  MetaVarName<"<tarfilename>">,
                 Group<grp_extendedopts>;
defm reproduce_compressed : smDashWithOpt<"reproduce-compressed", "reproduce_compressed",
                                   "Write compressed tar file in zlib format "
                                   "containing input files and command line "
                                   "options to inspect and re-link.">,
                            MetaVarName<"<tarfilename>">,
                            Group<grp_extendedopts>;
defm reproduce_on_fail : smDashWithOpt<"reproduce-on-fail", "reproduce_on_fail",
                                   "Write reproduce tar file when link fails"
                                   "containing input files and command line "
                                   "options to inspect and re-link.">,
                            MetaVarName<"<tarfilename>">,
                            Group<grp_extendedopts>;
defm mapping_file : smDashWithOpt<"mapping-file", "mapping_file",
                                  "Reproduce link using a mapping file">,
                    MetaVarName<"<INI-file>">,
                    Group<grp_extendedopts>;
defm dump_mapping_file : smDashWithOpt<"dump-mapping-file", "dump_mapping_file",
                                       "Dump mapping file to output file">,
                         MetaVarName<"<outputfilename>">,
                         Group<grp_extendedopts>;
defm dump_response_file
    : smDashWithOpt<"dump-response-file", "dump_response_file",
                    "Dump response file to output file">,
                    MetaVarName<"<outputfilename>">,
      Group<grp_extendedopts>;
def about :
     Flag<["--"], "about">,
      HelpText<"Emit detailed information about the linker">,
      Group<grp_extendedopts>;


//===----------------------------------------------------------------------===//
/// Map file
//===----------------------------------------------------------------------===//
def grp_mapopts : OptionGroup<"opts">, HelpText<"Map Options">;
def color_map : Flag<["--"], "color-map">,
                HelpText<"Color the map file">,
                Group<grp_mapopts>;
def MapText : Flag<["-"], "M">,
              HelpText<"Emit the map file">,
              Group<grp_mapopts>;
def PrintMap : Flag<["--"], "print-map">, Alias<MapText>;
defm Map : dashEq<"Map", "Map", "Dump the output layout to the map file">,
           MetaVarName<"<filename>">,
           Group<grp_mapopts>;
defm MapStyle
    : smDash<"MapStyle", "MapStyle",
             "Dump the output layout to the map file in YAML/Text/Binary Form">,
      MetaVarName<"<fileformat>">,
      Group<grp_mapopts>;
defm MapDetail
    : smDash<"MapDetail", "MapDetail", "Detail information in the map file">,
      MetaVarName<"<option>">,
      Group<grp_mapopts>;
defm TrampolineMap
    : smDashWithOpt<"trampoline-map", "TrampolineMap",
                    "Dump Trampoline Information in YAML format">,
      MetaVarName<"<filename>">,
      Group<grp_mapopts>;

//===----------------------------------------------------------------------===//
/// Compatibility or ignored options
//===----------------------------------------------------------------------===//
def grp_compatorignoredopts : OptionGroup<"opts">,
                              HelpText<"Compatibility Or Ignored Options">;
def Qy : Flag<["-"], "Qy">,
         HelpText<"This option is ignored for SVR4 compatibility">,
         Group<grp_compatorignoredopts>;
def nmagic : Flag<["--"], "nmagic">,
             HelpText<"Turn off page alignment of sections,"
                      " and disable linking against shared libraries">,
             Group<grp_compatorignoredopts>;
// Compatible Aliases
def nmagic_alias : Flag<["-"], "n">,
                   HelpText<"Turn off page alignment of sections,"
                            " and disable linking against shared libraries">,
                   Alias<nmagic>,
                   Group<grp_compatorignoredopts>;
def add_needed : Flag<["--"], "add-needed">,
                 Group<grp_compatorignoredopts>,
                 HelpText<"Deprecated">;
def no_add_needed : Flag<["--"], "no-add-needed">,
                    Group<grp_compatorignoredopts>,
                    HelpText<"Deprecated">;
def copy_dt_needed
    : Flag<["--"], "copy-dt-needed-entries">,
      Group<grp_compatorignoredopts>,
      HelpText<"Add the dynamic libraries mentioned to DT_NEEDED">;
def no_copy_dt_needed
    : Flag<["--"], "no-copy-dt-needed-entries">,
      Group<grp_compatorignoredopts>,
      HelpText<"Turn off the effect of the --copy-dt-needed-entries">;
def EL : Flag<["-"], "EL">,
         HelpText<"Link little endian objects.">,
         Group<grp_compatorignoredopts>;
def ignore_unknown_opts : Flag<["--"], "ignore-unknown-opts">,
                          HelpText<"Disable warnings of unimplemented options">,
                          Group<grp_compatorignoredopts>;
def no_allow_shlib_undefs
    : Flag<["--"], "no-allow-shlib-undefined">,
      HelpText<"Do not allow undefined symbols from dynamic"
               " library when creating executables">,
      Group<grp_compatorignoredopts>;
defm plugin_gnu
    : smDashWithOpt<"plugin", "plugin_gnu", "Specify a plugin file">,
    MetaVarName<"<pluginfile>">,
      Group<grp_compatorignoredopts>;
defm plugin_opt
    : smDashWithOpt<"plugin-opt", "plugin_opt", "Specify plugin options">,
      MetaVarName<"<plugin-opt>">,
      Group<grp_compatorignoredopts>;
def thin_archive_rule_matching_compatibility
    : Flag<["-", "--"], "thin-archive-rule-matching-compatibility">,
      HelpText<"Provides rule-matching compatibility when fat archives are\n"
               "\t\t\tconverted to thin archives">,
      Group<grp_compatorignoredopts>;

//===----------------------------------------------------------------------===//
/// LTO options
//===----------------------------------------------------------------------===//
def grp_ltooptions : OptionGroup<"opts">, HelpText<"LTO Options">;
def plugin_opt_eq_minus
    : J<"plugin-opt=-">,
      HelpText<"Specify an LLVM option for compatibility with LLVMgold.so">,
      Group<grp_ltooptions>;
def flto : Flag<["-", "--"], "flto">,
           HelpText<"Enable LTO if a Bitcode file is present.">,
           Group<grp_ltooptions>;
defm flto_options
    : dashEqWithOpt<"flto-options", "flto-options", "flto_options",
                    "Specify various options with LTO">,
      MetaVarName<"<option>">,
      Group<grp_ltooptions>;
def flto_use_as : Flag<["-", "--"], "flto-use-as">,
                  HelpText<"Use the standalone assembler instead of the "
                           "integrated assembler for LTO">,
                  Group<grp_ltooptions>;
def save_temps : Flag<["-", "--"], "save-temps">,
                 HelpText<"Save the temporary files produced by LTO">,
                 Group<grp_ltooptions>;
defm save_temps_EQ : smDashOnlyEq<"save-temps", "save_temps_EQ",
                 "Save the temporary files produced by LTO">,
                 MetaVarName<"<filename>">,
                 Group<grp_ltooptions>;
defm exclude_lto_filelist
    : smDashWithOpt<"exclude-lto-filelist", "exclude_lto_filelist",
                    "Specify a list of files that are to be disregarded while "
                    "using embedded bitcode section for LTO. This has no "
                    "effect if -flto switch is not used.">,
      MetaVarName<"<list_of_files>">,
      Group<grp_ltooptions>;
defm include_lto_filelist
    : smDashWithOpt<
          "include-lto-filelist", "include_lto_filelist",
          "Specify a list of files with embedded bitcode sections that are to "
          "be used for LTO. This has no effect if -flto switch is used">,
      MetaVarName<"<list_of_files>">,
      Group<grp_ltooptions>;
defm dwodir
    : smDash<"dwodir", "dwodir",
             "Directory to save .dwo files when split DWARF is used in LTO">,
      Group<grp_ltooptions>;

def lto_O : JJ<"lto-O">,
            MetaVarName<"<opt-level>">,
            HelpText<"Optimization level for LTO">;
def: J<"plugin-opt=O">, Alias<lto_O>, HelpText<"Alias for --lto-O">;

defm lto_sample_profile
    : smDashOnlyEq<"lto-sample-profile", "lto_sample_profile",
                   "Sample PGO profile file to be loaded for LTO">,
      Group<grp_ltooptions>;

defm plugin_opt_sample_profile
    : smDashOnlyEqAlias<"plugin-opt=sample-profile", "lto_sample_profile",
                        "Alias for -lto-sample-profile=">,
      Group<grp_ltooptions>;

//===----------------------------------------------------------------------===//
/// Linker speedup and control
//===----------------------------------------------------------------------===//
def grp_linktime : OptionGroup<"opts">, HelpText<"Link Time Speedup">;
def threads : Flag<["-", "--"], "threads">,
              HelpText<"Enable Threads at Link time">,
              Group<grp_linktime>;
defm enable_threads
    : smDashOnlyEq<"enable-threads", "enable_threads",
                   "make linker fully threaded, available options: all">,
      MetaVarName<"<option>">,
      Group<grp_linktime>;
def no_threads : Flag<["-", "--"], "no-threads">,
                 HelpText<"Disable Threads at Link time">,
                 Group<grp_linktime>;
defm thread_count
    : mDashEq<"thread-count", "thread_count",
              "Specify the number of threads for all linker operations">,
      MetaVarName<"<threadcount>">,
      Group<grp_linktime>;

//===----------------------------------------------------------------------===//
/// Features from Other Linkers.
//===----------------------------------------------------------------------===//
def grp_otherlinker : OptionGroup<"opts">,
                      HelpText<"Features From other Linkers">;
def symdef : Flag<["-", "--"], "symdef">,
             HelpText<"Output SymDef file to console">,
             Group<grp_otherlinker>;
defm symdef_file : mDashEq<"symdef-file", "symdef_file", "Emit SymDef file">,
                   MetaVarName<"<filename>">,
                   Group<grp_otherlinker>;
defm symdef_style
    : mDashEq<
          "symdef-style", "symdef_style",
          "Determine how to handle symbol resolution for symbols from symdef file, Options available are "
          "provide">,
      MetaVarName<"<style>">,
      Group<grp_otherlinker>;
defm R : smDash<"R", "just-symbols",
                "Read symbol names and addresses from filename">,
         MetaVarName<"<filename>">,
         Group<grp_otherlinker>;

//===----------------------------------------------------------------------===//
/// Plugin Features
//===----------------------------------------------------------------------===//
def grp_pluginopts : OptionGroup<"pluginopts">, HelpText<"Plugin Options">;
defm plugin_config : mDashEq<"plugin-config", "plugin_config",
                             "Specify a plugin configuration">,
                     MetaVarName<"<config-file>">,
                     Group<grp_pluginopts>;
def noDefaultPlugins
    : Flag<["--"], "no-default-plugins">,
      HelpText<"Allow no plugins to be implicitly loaded">,
      Group<grp_pluginopts>;

//===----------------------------------------------------------------------===//
/// Other Features
//===----------------------------------------------------------------------===//
def grp_miscfeatures : OptionGroup<"otherfeatures">, HelpText<"Misc Features">;
def allow_incompatible_section_mix
    : Flag<["-", "--"], "allow-incompatible-section-mix">,
      HelpText<"Allow incompatible section mix">,
      Group<grp_miscfeatures>;

//===----------------------------------------------------------------------===//
/// Help!
//===----------------------------------------------------------------------===//
def grp_help : OptionGroup<"opts">, HelpText<"Help!">;
def help : Flag<["--", "-"], "help">,
           HelpText<"Print option help">,
           Group<grp_help>;
def help_hidden : Flag<["--", "-"], "help-hidden">,
                  HelpText<"Print hidden option help">,
                  Group<grp_help>;

//===----------------------------------------------------------------------===//
/// Input formats!
//===----------------------------------------------------------------------===//
def grp_input_formats : OptionGroup<"opts">, HelpText<"Input Format">;
defm input_format : smDashTwoWithOpt<"b", "format", "input_format",
                      "Specify input format for inputs following this option.\n"
                      "Supported values: binary,default">,
                    MetaVarName<"<input-format>">,
                    Group<grp_input_formats>;
