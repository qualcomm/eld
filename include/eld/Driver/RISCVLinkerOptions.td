//===- RISCVLinkerOptions.td ----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "GnuLinkerOptions.td"

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

//===----------------------------------------------------------------------===//
/// RISCV specific options.
//===----------------------------------------------------------------------===//
def grp_riscv_linker : OptionGroup<"opts">, HelpText<"RISCV Linker Options">;

def no_riscv_relax : Flag<["--"], "no-relax">,
                     HelpText<"Disable relaxation">,
                     Group<grp_riscv_linker>;

def no_relax_zero : Flag<["--"], "no-relax-zero">,
                    HelpText<"Disable zero-page relaxation">,
                    Group<grp_riscv_linker>;

def no_relax_gp : Flag<["--"], "no-relax-gp">,
                  HelpText<"Disable GP relaxation">,
                  Group<grp_riscv_linker>;

def no_riscv_relax_compressed
    : Flag<["--"], "no-relax-c">,
      HelpText<"Disable relaxation to compressed instructions">,
      Group<grp_riscv_linker>;

def riscv_relax_xqci
    : Flag<["--"], "relax-xqci">,
      HelpText<"Enable relaxing to/from Xqci instructions">,
      Group<grp_riscv_linker>;
def no_riscv_relax_xqci
    : Flag<["--"], "no-relax-xqci">,
      HelpText<"Disable relaxing to/from Xqci instructions (default behavior)">,
      Group<grp_riscv_linker>;

def riscv_relax : Flag<["--"], "relax">,
                  HelpText<"Enable relaxation (default behavior)">,
                  Group<grp_riscv_linker>;
def enable_bss_mixing : Flag<["--"], "enable-bss-mixing">,
                        HelpText<"Enable mixing BSS/NonBSS sections">,
                        Group<grp_riscv_linker>;
def disable_bss_conversion
    : Flag<["--"], "disable-bss-conversion">,
      HelpText<
          "Don't convert BSS to NonBSS when BSS/NonBSS Sections are mixed">,
      Group<grp_riscv_linker>;

def keep_labels : Flag<["--"], "keep-labels">,
                  HelpText<"Keep all local labels for debugging purposes">,
                  Group<grp_riscv_linker>;

//===----------------------------------------------------------------------===//
/// Patching
//===----------------------------------------------------------------------===//
def grp_patch : OptionGroup<"opts">, HelpText<"Symbol Patching">;
def patch_enable : Flag<["--"], "patch-enable">,
                   HelpText<"Enable symbol patching using indirection">,
                   Group<grp_patch>;

defm patch_base : mDashEq<"patch-base", "patch_base",
                          "Specify base image to generate patch for">,
                  Group<grp_patch>;
