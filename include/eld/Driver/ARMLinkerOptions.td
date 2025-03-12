//===- ARMLinkerOptions.td ---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "GnuLinkerOptions.td"

//===----------------------------------------------------------------------===//
/// ARM Linker Options
//===----------------------------------------------------------------------===//
def grp_arm_linker : OptionGroup<"opts">,
                     HelpText<"ARM/AArch64 Linker Options">;
def enable_bss_mixing : Flag<["--"], "enable-bss-mixing">,
                        HelpText<"Enable mixing BSS/NonBSS sections">,
                        Group<grp_arm_linker>;
def disable_bss_conversion
    : Flag<["--"], "disable-bss-conversion">,
      HelpText<
          "Don't convert BSS to NonBSS when BSS/NonBSS Sections are mixed">,
      Group<grp_arm_linker>;
def use_mov_veneer
    : Flag<["--"], "use-mov-veneer">,
      HelpText<
          "Use movt/movw to load address in veneers with absolute relocation">,
      Group<grp_arm_linker>;
def compact : Flag<["-", "--"], "compact">,
              HelpText<"Create a smaller output file. The Loader need to "
                       "support such files">,
              Group<grp_arm_linker>;

def grp_arm_linker_only : OptionGroup<"opts">,
                          HelpText<"ARM Linker Option ONLY">;
def fix_cortex_a8 : Flag<["--"], "fix-cortex-a8">,
                    HelpText<"Fix the Cortex A8 bug">,
                    Group<grp_arm_linker_only>;
def no_fix_cortex_a8 : Flag<["--"], "no-fix-cortex-a8">,
                       HelpText<"Dont Fix the Cortex A8 bug">,
                       Group<grp_arm_linker_only>;
def fix_cortex_a53_843419 : Flag<["-", "--"], "fix-cortex-a53-843419">,
                            HelpText<"Fix the cortex a53 errata 843419">,
                            Group<grp_arm_linker_only>;
def frwpi : Flag<["-", "--"], "frwpi">,
            HelpText<"Enable Read write data access relative to a static base "
                     "registers. ">,
            Group<grp_arm_linker_only>;
def fropi : Flag<["-", "--"], "fropi">,
            HelpText<"Enable Read write data access relative to a static base "
                     "registers. ">,
            Group<grp_arm_linker_only>;

def execute_only : Flag<["-", "--"], "execute-only">,
                   HelpText<"Mark executable sections execute-only on AArch64">,
                   Group<grp_arm_linker_only>;

//===----------------------------------------------------------------------===//
/// Compatibility or ignored options
//===----------------------------------------------------------------------===//
def gpsize
    : Joined<["--"], "gpsize=">,
      MetaVarName<"<maxsize>">,
      Group<grp_compatorignoredopts>,
      HelpText<"Set the maximum size of objects to be optimized using GP">;
def gpsize_alias : JoinedOrSeparate<["-"], "G">,
                   MetaVarName<"<maxsize>">,
                   Group<grp_compatorignoredopts>,
                   Alias<gpsize>;
