//===- GnuLinkerOptions.td ------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "llvm/Option/OptParser.td"

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
/// Utility Functions
//===----------------------------------------------------------------------===//
// Single and multiple dash options combined
multiclass smDash<string opt1, string opt2, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(opt1)>;
  // Compatibility aliases
  def opt2_dashdash : Separate<["--"], opt2>, Alias<!cast<Option>(opt1)>;
  def opt2_dashdash_eq : JJ<opt2 #"=">, Alias<!cast<Option>(opt1)>;
}

//===----------------------------------------------------------------------===//
/// Deprecated aliases
//===----------------------------------------------------------------------===//
// Single dash option combined with deprecated multiple dash option with equals.
multiclass sDashDeprEq<string opt, string help> {
  // Option
  def "" : Separate<["-"], opt>, HelpText<help>;
  def opt_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(opt)>;

  // Deprecated aliases
  def opt_dashdash : Separate<["--"], opt>, Alias<!cast<Option>(opt)>,
                     HelpText<"Deprecated alias for -" # opt>;
  def opt_dashdash_eq : JJ<opt #"=">, Alias<!cast<Option>(opt)>,
                        HelpText<"Deprecated alias for -" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option.
multiclass mDashDepr<string opt, string help> {
  // Option
  def "" : FF<opt>, HelpText<help>;

  // Deprecated aliases
  def opt_dash : Flag<["-"], opt>, Alias<!cast<Option>(opt)>,
                 HelpText<"Deprecated alias for --" # opt>;
}

// Multiple dash option combined with deprecated single dash option.
multiclass mDashDeprWithOpt<string opt, string realopt, string help> {
  // Option
  def "" : FF<opt>, HelpText<help>;

  // Deprecated aliases
  def opt_dash : Flag<["-"], opt>, Alias<!cast<Option>(realopt)>,
                 HelpText<"Deprecated alias for --" # opt>;
}

// Multiple dash option combined with deprecated single dash option with equals.
multiclass mDashDeprEq<string opt, string help> {
  // Option
  def "" : Separate<["--"], opt>, HelpText<help>;
  def opt_eq : JJ<opt #"=">, Alias<!cast<Option>(opt)>;

  // Deprecated aliases
  def opt_dash : Separate<["-"], opt>, Alias<!cast<Option>(opt)>,
                 HelpText<"Deprecated alias for --" # opt>;
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(opt)>,
                    HelpText<"Deprecated alias for --" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option with equals.
multiclass mDashDeprEqWithOpt<string opt, string realopt, string help> {
  // Option
  def "" : Separate<["--"], opt>, HelpText<help>;
  def opt_eq : JJ<opt #"=">, Alias<!cast<Option>(realopt)>;

  // Deprecated aliases
  def opt_dash : Separate<["-"], opt>, Alias<!cast<Option>(realopt)>,
                 HelpText<"Deprecated alias for --" # opt>;
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(realopt)>,
                    HelpText<"Deprecated alias for --" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option with equals
// only.
multiclass mDashDeprEqOnlyWithOpt<string opt, string realopt, string help> {
  // Option
  def "" : JJ<opt #"=">, HelpText<help>;

  // Deprecated aliases
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(realopt)>,
                    HelpText<"Deprecated alias for --" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option with equals
// only aliases.
multiclass mDashDeprEqOnlyAlias<string opt, string realopt, string help> {
  // Option
  def "" : JJ<opt #"=">, Alias<!cast<Option>(realopt)>, HelpText<help>;

  // Deprecated aliases
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(realopt)>,
                    HelpText<"Deprecated alias for --" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option aliases.
multiclass mDashDeprAlias<string opt, string realopt, string help> {
  // Option
  def "" : FF<opt>, Alias<!cast<Option>(realopt)>, HelpText<help>;

  // Deprecated aliases
  def opt_dash_eq : Flag<["-"], opt>, Alias<!cast<Option>(realopt)>,
                    HelpText<"Deprecated alias for --" # opt>;
}

multiclass
    mDashDeprTwoWithOpt<string opt1, string opt2, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
  def opt1_joined : Joined<["-"], opt1>, Alias<!cast<Option>(realopt)>;
  def opt2_dashdash : Separate<["--"], opt2>, Alias<!cast<Option>(realopt)>;
  def opt2_dashdash_eq : JJ<opt2#"=">, Alias<!cast<Option>(realopt)>;

  // Deprecated aliases
  def opt1_dashdash : Separate<["--"], opt1>, Alias<!cast<Option>(realopt)>,
                      HelpText<"Deprecated alias for -" # opt1>;
  def opt1_dashdash_eq : JJ<opt1 #"=">,
                         Alias<!cast<Option>(realopt)>,
                         HelpText<"Deprecated alias for -" # opt1 # "=">;
  def opt2_dash : Separate<["-"], opt2>, Alias<!cast<Option>(realopt)>,
                  HelpText<"Deprecated alias for --" # opt2>;
  def opt2_dash_eq : Joined<["-"], opt2 #"=">, Alias<!cast<Option>(realopt)>,
                     HelpText<"Deprecated alias for --" # opt2 # "=">;
}

//===----------------------------------------------------------------------===//
/// Compatibility aliases
//===----------------------------------------------------------------------===//

// Multiple dash option combined with compatible single dash option.
multiclass mDashComp<string opt, string help> {
  // Option
  def "" : FF<opt>, HelpText<help>;

  // Compatibility aliases
  def opt_dash : Flag<["-"], opt>, Alias<!cast<Option>(opt)>,
                 HelpText<"Compatibility alias for --" # opt>;
}

// Multiple dash option combined with compatible single dash option.
multiclass mDashCompWithOpt<string opt, string realopt, string help> {
  // Option
  def "" : FF<opt>, HelpText<help>;

  // Compatibility aliases
  def opt_dash : Flag<["-"], opt>, Alias<!cast<Option>(realopt)>,
                 HelpText<"Compatibility alias for --" # opt>;
}

// Multiple dash option combined with compatible single dash option with equals.
multiclass mDashCompEq<string opt, string help> {
  // Option
  def "" : Separate<["--"], opt>, HelpText<help>;
  def opt_eq : JJ<opt #"=">, Alias<!cast<Option>(opt)>;

  // Compatibility aliases
  def opt_dash : Separate<["-"], opt>, Alias<!cast<Option>(opt)>,
                 HelpText<"Compatibility alias for --" # opt>;
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(opt)>,
                    HelpText<"Compatibility alias for --" # opt # "=">;
}

// Multiple dash option combined with compatible single dash option with equals.
multiclass mDashCompEqWithOpt<string opt, string realopt, string help> {
  // Option
  def "" : Separate<["--"], opt>, HelpText<help>;
  def opt_eq : JJ<opt #"=">, Alias<!cast<Option>(realopt)>;

  // Compatibility aliases
  def opt_dash : Separate<["-"], opt>, Alias<!cast<Option>(realopt)>,
                 HelpText<"Compatibility alias for --" # opt>;
  def opt_dash_eq : Joined<["-"], opt #"=">, Alias<!cast<Option>(realopt)>,
                    HelpText<"Compatibility alias for --" # opt # "=">;
}

// Multiple dash option combined with deprecated single dash option aliases.
multiclass mDashCompAlias<string opt, string realopt, string help> {
  // Option
  def "" : FF<opt>, Alias<!cast<Option>(realopt)>, HelpText<help>;

  // Compatibility aliases
  def opt_dash : Flag<["-"], opt>, Alias<!cast<Option>(realopt)>,
                 HelpText<"Compatibility alias for --" # opt>;
}

// Single and multiple dash options combined
multiclass smDashWithOpt<string opt1, string realopt, string help> {
  // Option
  def "" : Separate<["-"], opt1>, HelpText<help>;
  def opt1_eq : Joined<["-"], opt1 #"=">, Alias<!cast<Option>(realopt)>;
  // Compatibility aliases
  def opt1_dashdash : Separate<["--"], opt1>, Alias<!cast<Option>(realopt)>;
  def opt1_dashdash_eq : JJ<opt1 #"=">, Alias<!cast<Option>(realopt)>;
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
  def opt2_dashdash_eq : JJ<opt2 #"=">,
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
  def opt2_eq : JJ<opt1 #"=">, Alias<!cast<Option>(realopt)>;
}

// --<option>,--<option>=
multiclass EEq<string name, string help> {
  def NAME: Separate<["--"], name>;
  def NAME # _eq: JJ<name # "=">, Alias<!cast<Separate>(NAME)>,
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
  def opt1_eq : JJ<opt1 #"=">, Alias<!cast<Option>(realopt)>;
}

// Support -<option>=,--<option>= aliases
multiclass smDashOnlyEqAlias<string opt1, string realopt, string help> {
  def "" : Joined<["-"], opt1#"=">,
           Alias<!cast<Option>(realopt)>,
           HelpText<help>;
  def opt1_eq : JJ<opt1#"=">,
                Alias<!cast<Option>(realopt)>,
                HelpText<help>;
}

//===----------------------------------------------------------------------===//
/// General Options
//===----------------------------------------------------------------------===//
def grp_general : OptionGroup<"opts">, HelpText<"GENERAL OPTIONS">;
defm output_file : mDashDeprTwoWithOpt<"o", "output", "output_file",
                                       "Path to file to write output">,
                   MetaVarName<"<outputfile>">,
                   Group<grp_general>;
defm sysroot : mDashDeprEq<"sysroot", "Set the system root">,
               MetaVarName<"<sysroot-path>">,
               Group<grp_general>;
def version : FF<"version">,
              HelpText<"Print the Linker version">,
              Group<grp_general>;
def repository_version : FF<"repository-version">,
                         HelpText<"Print the Linker Repository version">,
                         Group<grp_general>;
def build_id : FF<"build-id">,
               HelpText<"Request creation of \".note.gnu.build-id\" ELF note "
                        "section">,
               Group<grp_general>;
defm build_id_val
    : mDashDeprEqOnlyWithOpt<"build-id", "build_id_val", "Request creation of "
                             "\".note.gnu.build-id\" ELF note section">,
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
                    Group<grp_llvmtarget>,
                    Alias<mtriple>;
def march : Separate<["-"], "march">,
            MetaVarName<"<march>">,
            HelpText<"Target architecture">,
            Group<grp_llvmtarget>;
def march_alias : Joined<["-"], "march=">,
                  MetaVarName<"<march>">,
                  HelpText<"Target architecture">,
                  Group<grp_llvmtarget>,
                  Alias<march>;
def mabi : Separate<["-"], "mabi">,
           MetaVarName<"<mabi>">,
           HelpText<"Target ABI">,
           Group<grp_llvmtarget>;
def mabi_alias : Joined<["-"], "mabi=">,
                 MetaVarName<"<mabi>">,
                 HelpText<"Target ABI">,
                 Group<grp_llvmtarget>,
                 Alias<mabi>;
def mcpu : Separate<["-"], "mcpu">,
           MetaVarName<"<mcpu>">,
           HelpText<"Target CPU">,
           Group<grp_llvmtarget>;
def mcpu_alias : Joined<["-"], "mcpu=">,
                 MetaVarName<"<mcpu>">,
                 Group<grp_llvmtarget>,
                 Alias<mcpu>;
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
defm T : mDashDeprTwoWithOpt<"T", "script", "T",
                             "Use the given linker script in place of the "
                             "default script.">,
         MetaVarName<"<linkerscriptfile>">,
         Group<grp_scriptopts>;
defm version_script : mDashDeprEqWithOpt<"version-script", "version_script",
                                         "Use the linker script as a version "
                                         "script.">,
                      MetaVarName<"<linkersciptfile>">,
                      Group<grp_scriptopts>;
defm default_script : mDashDeprEqWithOpt<"default-script", "default_script",
                                         "Use the linker script as the default "
                                         "linker script.">,
                      MetaVarName<"<pluginfile>">,
                      Group<grp_scriptopts>;
defm Ttext : mDashCompEq<"Ttext", "Specify an address for the .text section">,
             MetaVarName<"<address>">,
             Group<grp_scriptopts>;
defm Ttext_segment : mDashCompEqWithOpt<"Ttext-segment", "Ttext_segment",
                                          "Specify an address for the "
                                          ".text-segment segment">,
                     MetaVarName<"<address>">,
                     Group<grp_scriptopts>;
defm Tdata : mDashCompEq<"Tdata", "Specify an address for the .data section">,
             MetaVarName<"<address>">,
             Group<grp_scriptopts>;
defm Tbss : mDashCompEq<"Tbss", "Specify an address for the .bss section">,
            MetaVarName<"<address>">,
            Group<grp_scriptopts>;
defm dynamic_list : mDashDeprEqWithOpt<"dynamic-list", "dynamic_list",
                                       "Specify a list of symbols if present "
                                       "will be exported">,
                    MetaVarName<"<list-of-symbols>">,
                    Group<grp_scriptopts>;
defm extern_list : mDashDeprEqWithOpt<"extern-list", "extern_list",
                                      "Specify a list of symbols that exists "
                                      "as external dependencies">,
                   MetaVarName<"<list-of-symbols>">,
                   Group<grp_scriptopts>;
defm map_section : mDashEq<"map-section", "map_section",
                           "Specify a input section that maps to a output "
                           "section">,
                   MetaVarName<"<section>">,
                   Group<grp_scriptopts>;
defm section_start : mDashDeprEqWithOpt<"section-start", "section_start",
                                        "Specify a virtual output section "
                                        "address for a specified section">,
                     MetaVarName<"<address>">,
                     Group<grp_scriptopts>;
defm orphan_handling
    : mDashDeprEqWithOpt<"orphan-handling", "orphan_handling",
                         "Specify how to handle orphan sections. Options "
                         "available are place, error, warn">,
      MetaVarName<"<mode>">,
      Group<grp_scriptopts>;
defm exclude_libs : mDashEq<"exclude-libs", "exclude_libs",
                            "Specifies a list of archive libraries from which "
                            "symbols should not be automatically exported">,
                    MetaVarName<"<libs>">,
                    Group<grp_scriptopts>;
defm unique_output_sections
    : mDashDeprWithOpt<"unique-output-sections", "unique_output_sections",
                       "Place each input section in a unique output section">,
      Group<grp_scriptopts>;
def global_merge_non_alloc_strings
    : FF<"global-merge-non-alloc-strings">,
      HelpText<"merge non-alloc strings across output sections">,
      Group<grp_scriptopts>;
def disable_overlap_checks
    : FF<"no-check-sections">,
      HelpText<"Do not check section addresses for overlaps">,
      Group<grp_scriptopts>;
def enable_overlap_checks
    : FF<"check-sections">,
      HelpText<"Check section addresses for overlaps(default)">,
      Group<grp_scriptopts>;
def print_memory_usage
    : FF<"print-memory-usage">,
      HelpText<"Print memory usage when MEMORY linker script directive is used">,
      Group<grp_scriptopts>;

//===----------------------------------------------------------------------===//
/// Output Kinds
//===----------------------------------------------------------------------===//
def grp_kind : OptionGroup<"outs">, HelpText<"OUTPUT KIND">;
def relocatable : Flag<["-"], "r">,
                  HelpText<"Create relocatable object file">,
                  Group<grp_kind>;
defm static_link : mDashCompWithOpt<"static", "static_link",
                                    "Create static executable">,
                   Group<grp_kind>;
defm dynamic : mDashDepr<"dynamic", "Create dynamic executable (default)">,
               Group<grp_kind>;
defm shared : mDashComp<"shared", "Create dynamic library">,
              Group<grp_kind>;
defm pie : mDashComp<"pie", "Create PIE executable">,
           Group<grp_kind>;
defm no_pie : mDashCompWithOpt<"no-pie", "no_pie",
                               "Do not create a PIE executable">,
              Group<grp_kind>;
def picexec : FF<"pic-executable">,
              Group<grp_kind>,
              HelpText<"Create PIE executable">,
              Alias<pie>;
// output kinds - compatibility aliases
defm Bstatic : mDashCompAlias<"Bstatic", "static_link",
                              "Create static executable">,
               Group<grp_kind>;
defm dn : mDashCompAlias<"dn", "static_link", "Create static executable">,
          Group<grp_kind>;
defm non_shared : mDashCompAlias<"non_shared", "static_link",
                                 "Create static executable">,
                  Group<grp_kind>;
defm Bshareable : mDashCompAlias<"Bshareable", "shared",
                                 "Create dynamic library">,
                  Group<grp_kind>;
defm Bdynamic : mDashComp<"Bdynamic", "Link against dynamic library">,
                Group<grp_kind>;
defm BdynamicAlias1 : mDashCompAlias<"dy", "Bdynamic",
                                     "Link against dynamic library">,
                      Group<grp_kind>;
defm BdynamicAlias2 : mDashCompAlias<"call_shared", "Bdynamic",
                                     "Link against dynamic library">,
                      Group<grp_kind>;

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
def namespec : JJ<"library=">,
               MetaVarName<"<namespec>">,
               HelpText<"library to use">,
               Group<grp_main>;
defm library_path : mDashEq<"library-path", "library_path",
                            "Path to search for libraries or linker scripts">,
                    MetaVarName<"<path>">,
                    Group<grp_main>;
defm Y : sDashDeprEq<"Y", "Add path to the default library search path">,
         MetaVarName<"<path>">,
         Group<grp_main>;
def noinhibit_exec : FF<"noinhibit-exec">,
                     HelpText<"Retain the executable output file whenever "
                              "it is still usable">,
                     Group<grp_main>;
defm entrypoint : mDashDeprTwoWithOpt<"e", "entry", "entrypoint",
                                      "Name of entry point symbol">,
                  MetaVarName<"<symbolname>">,
                  Group<grp_main>;

defm image_base : EEq<"image-base", "Set the base address">,
                  MetaVarName<"<address>">,
                  Group<grp_main>;

defm init : mDashCompEq<"init", "Specify an initializer function">,
            MetaVarName<"<symbol>">,
            Group<grp_main>;
defm fini : mDashCompEq<"fini", "Specify a finalizer function">,
            MetaVarName<"<symbol>">,
            Group<grp_main>;
defm whole_archive : mDashDeprWithOpt<"whole-archive", "whole_archive",
                                      "Force load of all members in a static "
                                      "library">,
                     Group<grp_main>;
defm no_whole_archive : mDashDeprWithOpt<"no-whole-archive", "no_whole_archive",
                                         "Restores the default behavior of "
                                         "loading archive members">,
                        Group<grp_main>;
defm nostdlib : mDashComp<"nostdlib", "Disable default search path for "
                          "libraries">,
                Group<grp_main>;
defm emit_relocs : mDashCompWithOpt<"emit-relocs", "emit_relocs",
                                      "Make Emit relocs behave just like GNU.">,
                   Group<grp_main>;
def omagic
    : FF<"omagic">,
      HelpText<"Set the text and data sections to be readable and writable. "
               "Also, do not page-align the data segment, and "
               "disable linking against shared libraries.">,
      Group<grp_main>;
def no_omagic
    : FF<"no-omagic">,
      HelpText<"This option negates most of the effects of the -N option. "
               "Disable linking with shared libraries">,
      Group<grp_main>;
def omagic_alias
    : Flag<["-"], "N">,
      HelpText<"Set the text and data sections to be readable and writable. "
               "Also, do not page-align the data segment, and "
               "disable linking against shared libraries.">,
      Alias<omagic>,
      Group<grp_main>;

//===----------------------------------------------------------------------===//
/// Dynamic Library/Executable Options
//===----------------------------------------------------------------------===//
def grp_dynlibexec : OptionGroup<"opts">,
                     HelpText<"DYNAMIC LIBRARY/EXECUTABLE OPTIONS">;
defm dynamic_linker : mDashDeprEqWithOpt<"dynamic-linker", "dynamic_linker",
                                         "Set the path to the dynamic linker">,
                      MetaVarName<"<path>">,
                      Group<grp_dynlibexec>;
defm rpath : mDashCompEq<"rpath",
                         "Add a path to the runtime library search path">,
             MetaVarName<"<path>">,
             Group<grp_dynlibexec>;
defm rpath_link : mDashCompEqWithOpt<"rpath-link", "rpath_link",
                                     "Specifies the path to search">,
                  MetaVarName<"<path>">,
                  Group<grp_dynlibexec>;
defm export_dynamic : mDashDeprWithOpt<"export-dynamic", "export_dynamic",
                                       "Add all symbols to the dynamic symbol "
                                       "table when creating executables">,
                      Group<grp_dynlibexec>;
defm force_dynamic : mDashDeprWithOpt<"force-dynamic", "force_dynamic",
                                      "Build executable as a force dynamic "
                                      "executable">,
                     Group<grp_dynlibexec>;
def alias_export_dynamic : Flag<["-"], "E">,
                           HelpText<"Add all symbols to the dynamic symbol "
                                    "table when creating executables">,
                           Group<grp_dynlibexec>,
                           Alias<export_dynamic>;
def no_export_dynamic : FF<"no-export-dynamic">, Group<grp_dynlibexec>;
defm export_dynamic_symbol
    : mDashDeprEqWithOpt<"export-dynamic-symbol", "export_dynamic_symbol",
                         "Export specified symbol to dynamic symbol table">,
      MetaVarName<"<symbol>">,
      Group<grp_dynlibexec>;
defm no_dynamic_linker
    : mDashDeprWithOpt<"no-dynamic-linker", "no_dynamic_linker",
                       "The program does not need a dynamic linker when "
                       "creating static PIE executables">,
      Group<grp_dynlibexec>;

//===----------------------------------------------------------------------===//
/// Dynamic Library Options
//===----------------------------------------------------------------------===//
def grp_dynlib : OptionGroup<"opts">, HelpText<"DYNAMIC LIBRARY OPTIONS">;
defm soname : mDashCompEq<"soname", "Set the internal DT_SONAME field to the "
                          "specified name">,
              MetaVarName<"<name>">,
              Group<grp_dynlib>;
def soname_h : JoinedOrSeparate<["-"], "h">,
               HelpText<"Set the internal DT_SONAME field to the specified "
                        "name">,
               Alias<soname>,
               MetaVarName<"<name>">,
               Group<grp_dynlib>;
def dash_g : Flag<["-"], "g">,
             HelpText<"Enable debug output when building shared libraries or "
                      "executables">,
             Group<grp_dynlib>;
defm hash_size : mDashEq<"hash-size", "hash_size",
                         "Specify a hash size when creating the hash sections "
                         "for the dynamic loader">,
                 MetaVarName<"<size>">,
                 Group<grp_dynlib>;
defm hash_style : mDashDeprEqWithOpt<"hash-style", "hash_style",
                                     "Specify a hash style when creating the "
                                     "hash sections for the dynamic loader">,
                  MetaVarName<"<hashstyle>">,
                  Group<grp_dynlib>;
def warn_shared_textrel : FF<"warn-shared-textrel">,
                          HelpText<"Warn if the linker adds a DT_TEXTREL">,
                          Group<grp_dynlib>;
def no_warn_shared_textrel : FF<"no-warn-shared-textrel">,
                             HelpText<"Don't Warn if the linker adds a "
                                      "DT_TEXTREL">,
                             Group<grp_dynlib>;
defm Bsymbolic : mDashComp<"Bsymbolic", "Bind references to global symbols to "
                           "the definition within the shared library">,
                 Group<grp_dynlib>;
defm Bsymbolic_functions
    : mDashCompWithOpt<"Bsymbolic-functions", "Bsymbolic_functions", "Bind "
                       "references to global functions to the definition "
                       "within the shared library">,
      Group<grp_dynlib>;
defm Bgroup : mDashComp<"Bgroup", "Enable runtime linker to handle lookups in "
                        "this object and its dependencies to be performed only "
                        "inside this group">,
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
        HelpText<"Force symbol to be entered in the output file "
                 "as an undefined symbol">;
def u_alias : JJ<"undefined=">,
              HelpText<"Force symbol to be entered in the output file as an "
                       "undefined symbol">,
              MetaVarName<"<symbol>">,
              Group<grp_resolveropt>,
              Alias<u>;
defm start_group : mDashDeprWithOpt<"start-group", "start_group",
                                    "Start a group">,
                   Group<grp_resolveropt>;
def start_lib : FF<"start-lib">,
                HelpText<"Start a library">,
                Group<grp_resolveropt>;
def start_lib_thin : FF<"start-lib-thin">,
                     HelpText<"Start a thin library">,
                     Group<grp_resolveropt>;
def alias_start_group : Flag<["-"], "(">,
                        HelpText<"Start a group">,
                        Group<grp_resolveropt>,
                        Alias<start_group>;
defm end_group : mDashDeprWithOpt<"end-group", "end_group", "End a group">,
                 Group<grp_resolveropt>;
def end_lib : FF<"end-lib">,
              HelpText<"End a library">,
              Group<grp_resolveropt>;
def end_lib_thin : FF<"end-lib-thin">,
                   HelpText<"End a thin library">,
                   Group<grp_resolveropt>;
def alias_end_group : Flag<["-"], ")">,
                      HelpText<"End a group">,
                      Group<grp_resolveropt>,
                      Alias<end_group>;
def as_needed : FF<"as-needed">,
                HelpText<"This option affects ELF DT_NEEDED tags for "
                         "dynamic libraries mentioned on the command line">,
                Group<grp_resolveropt>;
def no_as_needed : FF<"no-as-needed">,
                   HelpText<"This option restores the default behavior "
                            "of adding DT_NEEDED entries">,
                   Group<grp_resolveropt>;
def push_state : FF<"push-state">,
                 HelpText<"Save current input handling flags (as-needed, "
                          "whole-archive, etc)">,
                 Group<grp_resolveropt>;
def pop_state : FF<"pop-state">,
                HelpText<"Restore previously saved input handling flags">,
                Group<grp_resolveropt>;
defm allow_shlib_undefs
    : mDashDeprWithOpt<"allow-shlib-undefined", "allow_shlib_undefs",
                       "Allow undefined symbols from dynamic library when "
                       "creating executables">,
      Group<grp_resolveropt>;
def use_shlib_undefs
    : FF<"use-shlib-undefines">,
      HelpText<"Resolve undefined symbols from dynamic libraries">,
      Group<grp_resolveropt>;
def allow_multiple_definition : FF<"allow-multiple-definition">,
                                HelpText<"Allow multiple definitions">,
                                Group<grp_resolveropt>;
defm defsym
    : mDashDeprEq<"defsym", "Create a global symbol in the output file "
                  "containing the absolute address given by expression">,
      MetaVarName<"symbol=<expression>">,
      Group<grp_resolveropt>;
defm no_undefined
    : mDashDeprWithOpt<"no-undefined", "no_undefined", "Report unresolved "
                       "symbol references from regular object files">,
      Group<grp_resolveropt>;
def warn_once : FF<"warn-once">,
                HelpText<"Warn only once for every undefined reference">,
                Group<grp_resolveropt>;
defm unresolved_symbols
    : mDashEq<"unresolved-symbols", "unresolved_symbols",
              "Determine how to handle unresolved symbols, Options available "
              "are ignore-all, report-all(Default), ignore-in-object-files, "
              "ignore-in-shared-libs">,
      MetaVarName<"<option>">,
      Group<grp_scriptopts>;
defm sort_section : EEq<"sort-section", "Sort sections. Options available are "
                        "name, alignment">,
                    MetaVarName<"<sort_section>">,
                    Group<grp_scriptopts>;

//===----------------------------------------------------------------------===//
/// Symbol options
//===----------------------------------------------------------------------===//
def grp_symbolopts : OptionGroup<"opts">, HelpText<"SYMBOL OPTIONS">;
defm demangle_style : mDashEq<"demangle-style", "demangle_style", "Specify "
                              "whether the linker should demangle symbols when "
                              "emitting errors or emitting Map files">,
                      Group<grp_symbolopts>;
def demangle : FF<"demangle">,
               HelpText<"Demangle C++ symbols">,
               Group<grp_symbolopts>;
def discard_locals : FF<"discard-locals">,
                     HelpText<"Discard all local symbols">,
                     Group<grp_symbolopts>;
def alias_discard_loc : Flag<["-"], "X">,
                        HelpText<"Discard all local symbols">,
                        Group<grp_symbolopts>,
                        Alias<discard_locals>;
def discard_all : FF<"discard-all">,
                  HelpText<"Discard all symbols">,
                  Group<grp_symbolopts>;
def x : Flag<["-"], "x">,
        HelpText<"Discard all symbols">,
        Group<grp_symbolopts>,
        Alias<discard_all>;
def strip_all : FF<"strip-all">,
                HelpText<"Omit all symbol information from output">,
                Group<grp_symbolopts>;
def s : Flag<["-"], "s">,
        HelpText<"Omit all symbol information from output">,
        Group<grp_symbolopts>,
        Alias<strip_all>;
def no_demangle : FF<"no-demangle">,
                  HelpText<"Don't demangle C++ symbols">,
                  Group<grp_symbolopts>;
def strip_debug : FF<"strip-debug">,
                  HelpText<"Omit all debug information from output">,
                  Group<grp_symbolopts>;
def S : Flag<["-"], "S">,
        HelpText<"Omit all debug information from output">,
        Group<grp_symbolopts>,
        Alias<strip_debug>;
defm wrap : mDashDeprEq<"wrap", "Specify symbol to be wrapped to">,
            MetaVarName<"<symbol>">,
            Group<grp_symbolopts>;
defm portable : mDashDeprEq<"portable",
                            "Specify symbol to be portable wrapped to">,
                MetaVarName<"<symbol>">,
                Group<grp_symbolopts>;
def d : Flag<["-"], "d">,
        HelpText<"Assign space to common symbols">,
        Group<grp_symbolopts>;
def dc : Flag<["-"], "dc">,
         HelpText<"Assign space to common symbols">,
         Alias<d>,
         Group<grp_symbolopts>;
def dp : Flag<["-"], "dp">,
         HelpText<"Assign space to common symbols">,
         Alias<d>,
         Group<grp_symbolopts>;
def sort_common : FF<"sort-common">,
                  HelpText<"sort common symbols by alignment">,
                  Group<grp_symbolopts>;
defm sort_common_val : mDashDeprEqOnlyWithOpt<"sort-common", "sort_common_val",
                                              "Sort common symbols by order of "
                                              "alignment">,
                       MetaVarName<"<ascending|descending>">,
                       Group<grp_symbolopts>;

//===----------------------------------------------------------------------===//
/// Diagnostic options
//===----------------------------------------------------------------------===//
def grp_diagopts : OptionGroup<"opts">, HelpText<"DIAGNOSITC OPTIONS">;
defm trace
    : mDashDeprEq<
          "trace",
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
          "\t\t\t --trace=untar : trace tar extraction entries\n"
          "\t\t\t --trace=wrap-symbols : trace symbol wrap options\n"
          "\t\t\t --trace=symdef : trace symbol resolution from symdef files\n"
          "\t\t\t --trace=dynamic-linking : trace dynamic linking\n"
          "\t\t\t --trace=symbol-versioning : trace symbol versioning">,
      MetaVarName<"<trace-type>">,
      Group<grp_diagopts>;
defm trace_symbol : mDashDeprTwoWithOpt<"y", "trace-symbol", "trace_symbol",
                                        "Trace a particular symbol">,
                    MetaVarName<"<symbol>">,
                    Group<grp_diagopts>;
def trace_lto : FF<"trace-lto">,
                HelpText<"Trace stages of lto">,
                Group<grp_diagopts>;
def trace_linker_script : FF<"trace-linker-script">,
                HelpText<"Trace stages of linker script processing">,
                Group<grp_diagopts>;
defm trace_section
    : mDashDeprEqWithOpt<"trace-section", "trace_section",
                         "Show metadata about a particular section">,
      MetaVarName<"<section_name>">,
      Group<grp_diagopts>;
defm trace_reloc : mDashDeprEqWithOpt<"trace-reloc", "trace_reloc",
                                      "trace a particular relocation">,
                   MetaVarName<"<relocation>">,
                   Group<grp_diagopts>;
defm trace_merge_strings
    : mDashDeprEqWithOpt<"trace-merge-strings", "trace_merge_strings",
          "Emit diagnostics describing how strings were merged. Options: "
          "all(default), allocatable_sections, <output section (regex)>">,
      MetaVarName<"<option>">,
      Group<grp_diagopts>;
defm verify_options
    : mDashDeprEqWithOpt<"verify-options", "verify_options",
                         "Verify certain internal linker "
                         "computations - currently supports reloc">,
      MetaVarName<"<option>">,
      Group<grp_diagopts>;
def dash_t : Flag<["-"], "t">,
             HelpText<"Print all files processed by the linker">,
             Group<grp_diagopts>;
defm cref : mDashDepr<"cref", "Print the references for a symbol or section">,
            Group<grp_diagopts>;
defm emit_timing_stats : mDashEq<"emit-timing-stats", "emit_timing_stats",
                                 "Emit time statistics of various linker "
                                 "operations to the specified file">,
                         MetaVarName<"<filename>">,
                         Group<grp_diagopts>;

defm emit_timing_stats_in_output
    : mDashDeprWithOpt<"emit-timing-stats-in-output",
                       "emit_timing_stats_in_output",
                       "Insert link-time stats in section .note.qc.timing">,
      Group<grp_diagopts>;

defm time_region : mDashEq<"time-region", "time_region",
                           "Emit time statistics for specified region of "
                           "linker operation ">,
                   MetaVarName<"<region>">,
                   Group<grp_diagopts>;

defm print_timing_stats
    : mDashDeprWithOpt<"print-timing-stats", "print_timing_stats",
                       "Print time statistics of various linker operations to "
                       "console">,
      Group<grp_diagopts>;
defm gc_cref : mDashEq<"gc-cref", "gc_cref", "Print the references for a "
                       "symbol or section when garbage collection is enabled">,
               MetaVarName<"<symbol/section>">,
               Group<grp_diagopts>;
def verbose : FF<"verbose">,
              HelpText<"Enable verbose output">,
              Group<grp_diagopts>;
defm verbose_level : mDashDeprEqOnlyWithOpt<"verbose", "verbose_level",
                                            "Enable verbose output">,
                     MetaVarName<"<verbose-level>">,
                     Group<grp_diagopts>;
def fatal_warnings : FF<"fatal-warnings">,
                     HelpText<"Enable fatal warnings">,
                     Group<grp_diagopts>;
def no_fatal_warnings : FF<"no-fatal-warnings">,
                        HelpText<"Disable fatal warnings">,
                        Group<grp_diagopts>;
def warn_common : FF<"warn-common">,
                  HelpText<"Warn on common symbols">,
                  Group<grp_diagopts>;
def no_warn_mismatch : FF<"no-warn-mismatch">,
                       HelpText<"Do not Warn on incompatible files passed to "
                                "the linker.">,
                       Group<grp_diagopts>;
def warn_mismatch
    : FF<"warn-mismatch">,
      HelpText<"Warn on incompatible files passed to the linker.">,
      Group<grp_diagopts>;
defm error_style : mDashEq<"error-style", "error_style", "Specify an error "
                           "style, LLVM/GNU">,
                   MetaVarName<"<style>">,
                   Group<grp_diagopts>;
defm color : mDashDeprEq<"color", "Enable color output for diagnostics">,
             MetaVarName<"<never/always/auto>">,
             Group<grp_diagopts>;
defm opt_record_file : mDashDeprWithOpt<"opt-record-file", "opt_record_file",
                                        "Create diagnostic yaml file">,
                       Group<grp_diagopts>;
defm display_hotness : mDashDeprEqWithOpt<"display-hotness", "display_hotness",
                                        "Display hotness information for "
                                        "optimization remarks">,
                       Group<grp_diagopts>;
def progress_bar : FF<"progress-bar">,
                   HelpText<"Show Progress Bar">,
                   Group<grp_diagopts>;
def summary : FF<"summary">,
              HelpText<"Display linker run summary at the end">,
              Group<grp_diagopts>;
def fatal_internal_errors : FF<"fatal-internal-errors">,
                            HelpText<"Enable fatal internal errors">,
                            Group<grp_diagopts>;
def no_fatal_internal_errors : FF<"no-fatal-internal-errors">,
                               HelpText<"Disable fatal internal errors">,
                               Group<grp_diagopts>;
defm error_limit : mDashEq<"error-limit", "error_limit", "Maximum number of "
                           "errors to emit before stopping (0 = no limit)">,
                   MetaVarName<"<max-error-number>">,
                   Group<grp_diagopts>;
defm warn_limit : mDashEq<"warn-limit", "warn_limit",
                          "Maximum number of warnings to emit (0 = no limit)">,
                  MetaVarName<"<maxwarnings>">,
                  Group<grp_diagopts>;
def W : Joined<["-"], "W">,
        MetaVarName<"<warn-type>">,
        HelpText<"Print warnings which are OFF by default\n"
             "\t\t\t -Wall : print all warnings\n"
             "\t\t\t -W[no]linker-script : display warnings detected "
             "while parsing linker scripts\n"
             "\t\t\t -W[no]attribute-mix : display a warning if object files "
             "of RISC-V with different attributes are being linked\n"
             "\t\t\t -W[no-]archive-file : display warnings detected while "
             "reading archive files\n"
             "\t\t\t -W[no-]linker-script-memory : display warnings detected "
             "while processing linker script memory command\n"
             "\t\t\t -W[no-]bad-dot-assignments : display warnings for bad "
             "dot assignments\n"
             "\t\t\t -W[no-]error : treat warnings as errors\n"
             "\t\t\t -W[no-]whole-archive : display a warning when "
             "whole-archive is enabled for any archive file\n"
             "\t\t\t -W[no-]osabi : display a warning when linking objects "
             "with different values for OS/ABI\n"
             "\t\t\t -W[no-]version-script : display warnings detected while "
             "processing version scripts\n"
            >,
        Group<grp_diagopts>;

//===----------------------------------------------------------------------===//
/// Optimization options
//===----------------------------------------------------------------------===//
def grp_optimizationopts : OptionGroup<"opts">,
                           HelpText<"OPTIMIZATION OPTIONS">;
defm gc_sections : mDashDeprWithOpt<"gc-sections", "gc_sections",
                                    "Enable garbage collection">,
                   Group<grp_optimizationopts>;
defm no_gc_sections : mDashDeprWithOpt<"no-gc-sections", "no_gc_sections",
                                       "Disable garbage collection">,
                     Group<grp_optimizationopts>;
defm print_gc_sections
    : mDashDeprWithOpt<"print-gc-sections", "print_gc_sections",
                       "Print sections that are garbage collected">,
      Group<grp_optimizationopts>;
def eh_frame_hdr : FF<"eh-frame-hdr">,
                   HelpText<"Create EH Frame Header section for faster "
                            "exception handling">,
                   Group<grp_optimizationopts>;
def sframe_hdr
    : FF<"sframe-hdr">,
      HelpText<"Process and emit .sframe sections for stack unwinding">,
      Group<grp_optimizationopts>;
def no_merge_strings : FF<"no-merge-strings">,
                       HelpText<"Disable String Merging">,
                       Group<grp_optimizationopts>;
def no_trampolines : FF<"no-trampolines">,
                     HelpText<"Disable Trampolines">,
                     Group<grp_optimizationopts>;

//===----------------------------------------------------------------------===//
/// Extended Options
//===----------------------------------------------------------------------===//
def grp_extendedopts : OptionGroup<"opts">, HelpText<"Extended Options">;
defm dash_z
    : dashEqWithJoinedOpt<
          "z", "z", "dash_z",
          "Extended Options or Non standard options.\n\t\t\t Available "
          "options are:\n"
          "\t\t\t-z=combreloc : Combines multiple reloc sections and sorts "
          "them to make dynamic symbol lookup caching\n"
          "\t\t\t-z=now : Enables immediate binding\n"
          "\t\t\t-z=nocopyreloc : Disables Copy Relocation\n"
          "\t\t\t-z=text : Do not permit relocations in read-only segments "
          "(default)\n"
          "\t\t\t-z=notext : Allow relocations in read-only segments\n"
          "\t\t\t-z=separate-code : Create separate code segment\n"
          "\t\t\t-z=no-separate-code : Do not create separate code segment\n"
          "\t\t\t-z=separate-loadable-segments : Create separate loadable "
          "segments\n">,
      MetaVarName<"<extended-opts>">,
      Group<grp_extendedopts>;
def no_align_segments : FF<"no-align-segments">,
                        HelpText<"Don't align segments to page boundaries">,
                        Group<grp_extendedopts>;
defm emit_relocs_llvm : mDashDeprWithOpt<"emit-relocs-llvm", "emit_relocs_llvm",
                                         "Emit relocations sections">,
                        Group<grp_extendedopts>;
def align_segments : FF<"align-segments">,
                     HelpText<"Align segments to page boundaries">,
                     Group<grp_extendedopts>;
def no_verify : FF<"no-verify">,
                HelpText<"Don't verify link output">,
                Group<grp_extendedopts>;
def disable_newdtags : FF<"disable-new-dtags">,
                       HelpText<"Disable new dynamic tags">,
                       Group<grp_extendedopts>;
def enable_newdtags : FF<"enable-new-dtags">,
                      HelpText<"Enable new dynamic tags">,
                      Group<grp_extendedopts>;
def disable_linker_version : FF<"disable-linker-version">,
                             HelpText<"Disable the LINKER_VERSION linker "
                                      "script directive (default)">,
                             Group<grp_extendedopts>;
def enable_linker_version : FF<"enable-linker-version">,
                            HelpText<"Enable the LINKER_VERSION linker script "
                                     "directive">,
                            Group<grp_extendedopts>;
def record_command_line : FF<"record-command-line">,
                          HelpText<"Record the linker command line in the "
                                    "comment section">,
                          Group<grp_extendedopts>;
def no_record_command_line : FF<"no-record-command-line">,
                             HelpText<"Do not record the linker command line "
                                      "in the comment section">,
                             Group<grp_extendedopts>;
defm rosegment : mDashDepr<"rosegment", "Put read-only non-executable sections "
                           "in their own segment">,
                 Group<grp_extendedopts>;
defm copy_farcalls_from_file
    : mDashDeprEqWithOpt<"copy-farcalls-from-file", "copy_farcalls_from_file",
                         "Copy far calls instead of using trampolines">,
      MetaVarName<"<filename>">,
      Group<grp_extendedopts>;
defm no_reuse_trampolines_file
    : mDashDeprEqWithOpt<"no-reuse-trampolines-file",
                         "no_reuse_trampolines_file",
                         "Don't reuse trampolines for symbols specified in "
                         "file">,
      MetaVarName<"<filename>">,
      Group<grp_extendedopts>;
def use_old_style_trampoline_name
    : FF<"use-old-style-trampoline-name">,
      HelpText<"Use old style of naming trampolines">,
      Group<grp_extendedopts>;
defm no_emit_relocs
    : mDashDeprWithOpt<"no-emit-relocs", "no_emit_relocs", "Don't emit "
                       "relocations in the output file, applicable to "
                       "--emit-relocs-llvm option too">,
      Group<grp_extendedopts>;
def use_old_rule_matching
    : FF<"use-old-rule-matching">,
      HelpText<"Use the original (pre-optimization) linker script "
               "rule-matching algorithm">,
      Group<grp_extendedopts>;
defm script_options : mDashEq<"script-options", "script_options",
                              "Specify a match type, match-gnu/match-llvm">,
                      MetaVarName<"<matchtype>">,
                      Group<grp_extendedopts>;
def allow_bss_conversion
    : FF<"allow-bss-conversion">,
      HelpText<"Don't produce an error when mixing NOBITS, and PROGBITS "
               "sections in the same segment">,
      Group<grp_extendedopts>;
defm reproduce : mDashDeprEq<"reproduce",
                             "Write a tar file containing input files and "
                             "command line options to inspect and re-link">,
                 MetaVarName<"<tarfilename>|default">,
                 Group<grp_extendedopts>;
defm reproduce_compressed
    : mDashDeprEqWithOpt<"reproduce-compressed", "reproduce_compressed",
                         "Write compressed tar file in zlib format "
                         "containing input files and command line "
                         "options to inspect and re-link.">,
      MetaVarName<"<tarfilename>">,
      Group<grp_extendedopts>;
defm reproduce_on_fail
    : mDashDeprEqWithOpt<"reproduce-on-fail", "reproduce_on_fail",
                         "Write reproduce tar file when link fails "
                         "containing input files and command line "
                         "options to inspect and re-link.">,
     MetaVarName<"<tarfilename>|default">,
     Group<grp_extendedopts>;
defm mapping_file : mDashDeprEqWithOpt<"mapping-file", "mapping_file",
                                       "Reproduce link using a mapping file">,
                    MetaVarName<"<INI-file>">,
                    Group<grp_extendedopts>;
defm dump_mapping_file : mDashDeprEqWithOpt<"dump-mapping-file",
                                            "dump_mapping_file",
                                            "Dump mapping file to output file">,
                         MetaVarName<"<outputfilename>">,
                         Group<grp_extendedopts>;
defm dump_response_file
    : mDashDeprEqWithOpt<"dump-response-file", "dump_response_file",
                         "Dump response file to output file">,
      MetaVarName<"<outputfilename>">,
      Group<grp_extendedopts>;
def about : FF<"about">,
            HelpText<"Emit detailed information about the linker">,
            Group<grp_extendedopts>;


//===----------------------------------------------------------------------===//
/// Map file
//===----------------------------------------------------------------------===//
def grp_mapopts : OptionGroup<"opts">, HelpText<"Map Options">;
def color_map : FF<"color-map">,
                HelpText<"Color the map file">,
                Group<grp_mapopts>;
def MapText : Flag<["-"], "M">,
              HelpText<"Emit the map file">,
              Group<grp_mapopts>;
def PrintMap : FF<"print-map">, Group<grp_mapopts>, Alias<MapText>;
defm Map : mDashCompEq<"Map", "Dump the output layout to the map file">,
           MetaVarName<"<filename>">,
           Group<grp_mapopts>;
defm MapStyle : mDashDeprEq<"MapStyle", "Dump the output layout to the map "
                            "file in YAML/Text/Binary Form">,
                MetaVarName<"<fileformat>">,
                Group<grp_mapopts>;
defm MapDetail : mDashDeprEq<"MapDetail", "Detail information in the map file">,
                 MetaVarName<"<option>">,
                 Group<grp_mapopts>;
defm TrampolineMap
    : mDashDeprEqWithOpt<"trampoline-map", "TrampolineMap", "Dump Trampoline "
                         "Information in YAML format">,
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
def nmagic : FF<"nmagic">,
             HelpText<"Turn off page alignment of sections, "
                      "and disable linking against shared libraries">,
             Group<grp_compatorignoredopts>;
// Compatible Aliases
def nmagic_alias : Flag<["-"], "n">,
                   HelpText<"Turn off page alignment of sections, "
                            "and disable linking against shared libraries">,
                   Alias<nmagic>,
                   Group<grp_compatorignoredopts>;
def add_needed : FF<"add-needed">,
                 Group<grp_compatorignoredopts>,
                 HelpText<"Deprecated">;
def no_add_needed : FF<"no-add-needed">,
                    Group<grp_compatorignoredopts>,
                    HelpText<"Deprecated">;
def copy_dt_needed
    : FF<"copy-dt-needed-entries">,
      Group<grp_compatorignoredopts>,
      HelpText<"Add the dynamic libraries mentioned to DT_NEEDED">;
def no_copy_dt_needed
    : FF<"no-copy-dt-needed-entries">,
      Group<grp_compatorignoredopts>,
      HelpText<"Turn off the effect of the --copy-dt-needed-entries">;
def EL : Flag<["-"], "EL">,
         HelpText<"Link little endian objects.">,
         Group<grp_compatorignoredopts>;
def ignore_unknown_opts : FF<"ignore-unknown-opts">,
                          HelpText<"Disable warnings of unimplemented options">,
                          Group<grp_compatorignoredopts>;
def no_allow_shlib_undefs
    : FF<"no-allow-shlib-undefined">,
      HelpText<"Do not allow undefined symbols from dynamic "
               "library when creating executables">,
      Group<grp_compatorignoredopts>;
defm plugin_gnu : mDashCompEqWithOpt<"plugin", "plugin_gnu",
                                     "Specify a plugin file">,
                  MetaVarName<"<pluginfile>">,
                  Group<grp_compatorignoredopts>;
defm plugin_opt : mDashCompEqWithOpt<"plugin-opt", "plugin_opt",
                                     "Specify plugin options">,
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
defm flto : mDashComp<"flto", "Enable LTO if a Bitcode file is present.">,
            Group<grp_ltooptions>;
defm fat_lto_objects : mDashDeprWithOpt<"fat-lto-objects", "fat_lto_objects",
                                        "Use .llvm.lto section in fat LTO "
                                        "objects">,
                       Group<grp_ltooptions>;
defm no_fat_lto_objects
    : mDashDeprWithOpt<"no-fat-lto-objects", "no_fat_lto_objects", "Ignore "
                       ".llvm.lto section in fat LTO objects">,
      Group<grp_ltooptions>;
defm ffat_lto_objects : mDashDeprAlias<"ffat-lto-objects", "fat_lto_objects",
                                       "Alias for --fat-lto-objects">,
                        Group<grp_ltooptions>;
defm fno_fat_lto_objects
    : mDashDeprAlias<"fno-fat-lto-objects", "no_fat_lto_objects",
                     "Alias for --no-fat-lto-objects">,
      Group<grp_ltooptions>;
defm flto_options : mDashDeprEqWithOpt<"flto-options", "flto_options",
                                       "Specify various options with LTO">,
                    MetaVarName<"<option>">,
                    Group<grp_ltooptions>;
defm flto_use_as : mDashDeprWithOpt<"flto-use-as", "flto_use_as", "Use the "
                                    "standalone assembler instead of the "
                                    "integrated assembler for LTO">,
                   Group<grp_ltooptions>;
def opt_remarks_filename
    : Separate<["--"], "opt-remarks-filename">,
      HelpText<"YAML output file for optimization remarks">,
      Group<grp_ltooptions>;
def : J<"plugin-opt=opt-remarks-filename=">,
      Alias<opt_remarks_filename>,
      HelpText<"Alias for --opt-remarks-filename">,
      Group<grp_ltooptions>;
def opt_remarks_format
    : Separate<["--"], "opt-remarks-format">,
      HelpText<"The format used for serializing remarks (default: YAML)">,
      Group<grp_ltooptions>;
def : J<"plugin-opt=opt-remarks-format=">,
      Alias<opt_remarks_format>,
      HelpText<"Alias for --opt-remarks-format">,
      Group<grp_ltooptions>;
defm opt_remarks_hotness_threshold
    : EEq<"opt-remarks-hotness-threshold",
          "Minimum profile count required for an optimization remark to be "
          "output. Use 'auto' to apply the threshold from profile summary.">,
      MetaVarName<"<value>">,
      Group<grp_ltooptions>;
def : J<"plugin-opt=opt-remarks-hotness-threshold=">,
      Alias<opt_remarks_hotness_threshold>,
      HelpText<"Alias for --opt-remarks-hotness-threshold">,
      Group<grp_ltooptions>;
def opt_remarks_passes : Separate<["--"], "opt-remarks-passes">,
                         HelpText<"Regex for the passes that need to be "
                                  "serialized to the output file">,
                         Group<grp_ltooptions>;
def : J<"plugin-opt=opt-remarks-passes=">,
      Alias<opt_remarks_passes>,
      HelpText<"Alias for --opt-remarks-passes">,
      Group<grp_ltooptions>;
def opt_remarks_with_hotness
    : FF<"opt-remarks-with-hotness">,
      HelpText<"Include hotness information in the optimization remarks file">,
      Group<grp_ltooptions>;
def : F<"plugin-opt=opt-remarks-with-hotness">,
      Alias<opt_remarks_with_hotness>,
      HelpText<"Alias for --opt-remarks-with-hotness">,
      Group<grp_ltooptions>;
defm save_temps : mDashDeprWithOpt<"save-temps", "save_temps",
                                   "Save the temporary files produced by LTO">,
                  Group<grp_ltooptions>;
defm save_temps_EQ
    : mDashDeprEqOnlyWithOpt<"save-temps", "save_temps_EQ",
                             "Save the temporary files produced by LTO in the "
                             "directory">,
      MetaVarName<"<dir>">,
      Group<grp_ltooptions>;
def : F<"plugin-opt=save-temps">,
      Alias<save_temps>,
      HelpText<"Alias for --save-temps">,
      Group<grp_ltooptions>;

def plugin_opt_stats_file : J<"plugin-opt=stats-file=">,
                            HelpText<"Filename to write LTO statistics to">,
                            Group<grp_ltooptions>;

defm exclude_lto_filelist
    : mDashDeprEqWithOpt<"exclude-lto-filelist", "exclude_lto_filelist",
                         "Specify a list of files that are to be disregarded "
                         "while using embedded bitcode section for LTO. This "
                         "has no effect if -flto switch is not used.">,
      MetaVarName<"<list_of_files>">,
      Group<grp_ltooptions>;
defm include_lto_filelist
    : mDashDeprEqWithOpt< "include-lto-filelist", "include_lto_filelist",
                          "Specify a list of files with embedded bitcode "
                          "sections that are to be used for LTO. This has no "
                          "effect if -flto switch is used">,
      MetaVarName<"<list_of_files>">,
      Group<grp_ltooptions>;

def lto_cs_profile_generate
    : FF<"lto-cs-profile-generate">,
      HelpText<"Perform context sensitive PGO instrumentation">,
      Group<grp_ltooptions>;
def lto_cs_profile_file : JJ<"lto-cs-profile-file=">,
                          HelpText<"Context sensitive profile file path">,
                          Group<grp_ltooptions>;

def : F<"plugin-opt=cs-profile-generate">,
      Alias<lto_cs_profile_generate>,
      HelpText<"Alias for --lto-cs-profile-generate">,
      Group<grp_ltooptions>;
def : J<"plugin-opt=cs-profile-path=">,
      Alias<lto_cs_profile_file>,
      HelpText<"Alias for --lto-cs-profile-file">,
      Group<grp_ltooptions>;

def disable_verify
    : F<"disable-verify">,
      HelpText<"Do not run the verifier during the optimization pipeline">,
      Group<grp_ltooptions>;
def : F<"plugin-opt=disable-verify">,
      Alias<disable_verify>,
      HelpText<"Alias for --disable-verify">,
      Group<grp_ltooptions>;

defm dwodir : mDashDeprEq<"dwodir", "Directory to save .dwo files when split "
                          "DWARF is used in LTO">,
              MetaVarName<"<dir>">,
              Group<grp_ltooptions>;

def lto_emit_asm : FF<"lto-emit-asm">,
                   HelpText<"Emit assembly code">,
                   Group<grp_ltooptions>;
def plugin_opt_emit_asm : F<"plugin-opt=emit-asm">,
                          Alias<lto_emit_asm>,
                          HelpText<"Alias for --lto-emit-asm">,
                          Group<grp_ltooptions>;

def lto_emit_llvm : FF<"lto-emit-llvm">,
                    HelpText<"Emit LLVM-IR bitcode">,
                    Group<grp_ltooptions>;
def plugin_opt_emit_llvm : F<"plugin-opt=emit-llvm">,
                           Alias<lto_emit_llvm>,
                           HelpText<"Alias for --lto-emit-llvm">,
                           Group<grp_ltooptions>;

def lto_obj_path_eq
    : JJ<"lto-obj-path=">,
      HelpText<"Specify file path used as a prefix for output LTO object "
               "files. Files created with this prefix will not be deleted "
               "after LTO finishes.">,
      MetaVarName<"<prefix>">,
      Group<grp_ltooptions>;
def : J<"plugin-opt=obj-path=">,
      Alias<lto_obj_path_eq>,
      HelpText<"Alias for --lto-obj-path=">,
      Group<grp_ltooptions>;

def thinlto_jobs_eq : JJ<"thinlto-jobs=">,
                      HelpText<"Number of ThinLTO jobs. Default to --threads=">,
                      MetaVarName<"<number>">,
                      Group<grp_ltooptions>;
def : J<"plugin-opt=jobs=">,
      Alias<thinlto_jobs_eq>,
      HelpText<"Alias for --thinlto-jobs=">,
      Group<grp_ltooptions>;

def lto_O : JJ<"lto-O">,
            MetaVarName<"<opt-level>">,
            HelpText<"Optimization level for LTO">,
            Group<grp_ltooptions>;
def : J<"plugin-opt=O">,
      Alias<lto_O>,
      HelpText<"Alias for --lto-O">,
      Group<grp_ltooptions>;

defm lto_sample_profile
    : mDashDeprEqOnlyWithOpt<"lto-sample-profile", "lto_sample_profile",
                             "Sample PGO profile file to be loaded for LTO">,
      MetaVarName<"<file>">,
      Group<grp_ltooptions>;

defm plugin_opt_sample_profile
    : mDashDeprEqOnlyAlias<"plugin-opt=sample-profile", "lto_sample_profile",
                           "Alias for --lto-sample-profile=">,
      Group<grp_ltooptions>;

def lto_partitions : JJ<"lto-partitions=">,
                     HelpText<"Number of LTO codegen partitions">,
                     MetaVarName<"<number>">,
                     Group<grp_ltooptions>;
def : J<"plugin-opt=lto-partitions=">,
      Alias<lto_partitions>,
      HelpText<"Alias for --lto-partitions">,
      Group<grp_ltooptions>;

def lto_debug_pass_manager : FF<"lto-debug-pass-manager">,
                             HelpText<"Debug new pass manager">,
                             Group<grp_ltooptions>;

def : F<"plugin-opt=debug-pass-manager">,
      Alias<lto_debug_pass_manager>,
      HelpText<"Alias for --lto-debug-pass-manager">,
      Group<grp_ltooptions>;

//===----------------------------------------------------------------------===//
/// Linker speedup and control
//===----------------------------------------------------------------------===//
def grp_linktime : OptionGroup<"opts">, HelpText<"Link Time Speedup">;
defm threads : mDashDepr<"threads", "Enable Threads at Link time">,
               Group<grp_linktime>;
defm enable_threads
    : mDashDeprEqOnlyWithOpt<"enable-threads", "enable_threads", "make linker "
                             "fully threaded, available options: all">,
      MetaVarName<"<option>">,
      Group<grp_linktime>;
defm no_threads : mDashDeprWithOpt<"no-threads", "no_threads",
                                   "Disable Threads at Link time">,
                  Group<grp_linktime>;
defm thread_count : mDashEq<"thread-count", "thread_count", "Specify the "
                            "number of threads for all linker operations">,
                    MetaVarName<"<threadcount>">,
                    Group<grp_linktime>;

//===----------------------------------------------------------------------===//
/// Features from Other Linkers.
//===----------------------------------------------------------------------===//
def grp_otherlinker : OptionGroup<"opts">,
                      HelpText<"Features From other Linkers">;
defm symdef : mDashDepr<"symdef", "Output SymDef file to console">,
              Group<grp_otherlinker>;
defm symdef_file : mDashEq<"symdef-file", "symdef_file", "Emit SymDef file">,
                   MetaVarName<"<filename>">,
                   Group<grp_otherlinker>;
defm symdef_style : mDashEq<"symdef-style", "symdef_style",
                            "Determine how to handle symbol resolution for "
                            "symbols from symdef file, Options available are "
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
def noDefaultPlugins : FF<"no-default-plugins">,
                       HelpText<"Allow no plugins to be implicitly loaded">,
                       Group<grp_pluginopts>;

defm PluginActivityFile
    : mDashDeprEqWithOpt<"plugin-activity-file", "PluginActivityFile",
                         "Emit plugin activity JSON files">,
      MetaVarName<"<filename>">,
      Group<grp_pluginopts>;

//===----------------------------------------------------------------------===//
/// Other Features
//===----------------------------------------------------------------------===//
def grp_miscfeatures : OptionGroup<"otherfeatures">, HelpText<"Misc Features">;
defm allow_incompatible_section_mix
    : mDashDeprWithOpt<"allow-incompatible-section-mix",
                       "allow_incompatible_section_mix",
                       "Allow incompatible section mix">,
      Group<grp_miscfeatures>;

defm ArchiveMemberReportFile
    : mDashDeprEqWithOpt<"archive-member-report", "ArchiveMemberReportFile",
                         "Emit JSON report for archive members pulled in by "
                         "symbols">,
      MetaVarName<"<filename>">,
      Group<grp_miscfeatures>;

//===----------------------------------------------------------------------===//
/// Help!
//===----------------------------------------------------------------------===//
def grp_help : OptionGroup<"opts">, HelpText<"Help!">;
def help : FF<"help">,
           HelpText<"Print option help">,
           Group<grp_help>;
def help_hidden : FF<"help-hidden">,
                  HelpText<"Print hidden option help">,
                  Group<grp_help>;

//===----------------------------------------------------------------------===//
/// Input formats!
//===----------------------------------------------------------------------===//
def grp_input_formats : OptionGroup<"opts">, HelpText<"Input Format">;
defm input_format
    : mDashDeprTwoWithOpt<"b", "format", "input_format",
                          "Specify input format for inputs following this "
                          "option.\n Supported values: binary,default">,
      MetaVarName<"<input-format>">,
      Group<grp_input_formats>;

defm remap_inputs : EEq<"remap-inputs",
                        "Remap input file names (pattern=filename)">,
                    MetaVarName<"<pattern=filename>">,
                    Group<grp_input_formats>;
defm remap_inputs_file : EEq<"remap-inputs-file",
                             "Read input file name remappings from file">,
                         MetaVarName<"<file>">,
                         Group<grp_input_formats>;
