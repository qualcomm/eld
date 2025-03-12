//===- HexagonLinkerOptions.td --------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "GnuLinkerOptions.td"

//===----------------------------------------------------------------------===//
/// Hexagon specific options.
//===----------------------------------------------------------------------===//
def grp_hexagonopts : OptionGroup<"opts">, HelpText<"Hexagon Options">;
def gpsize
    : Joined<["--"], "gpsize=">,
      MetaVarName<"<maxsize>">,
      Group<grp_hexagonopts>,
      HelpText<"Set the maximum size of objects to be optimized using GP">;
def gpsize_alias : JoinedOrSeparate<["-"], "G">,
                   MetaVarName<"<maxsize>">,
                   Group<grp_hexagonopts>,
                   Alias<gpsize>;
def disable_guard_for_weak_undef
    : Flag<["-", "--"], "disable-guard-for-weak-undef">,
      HelpText<"Disable guard for weak undefined symbols">,
      Group<grp_main>;

//===----------------------------------------------------------------------===//
/// Hexagon Linker Options (Experimental)
//===----------------------------------------------------------------------===//
def grp_hexagonexperimentalopts : OptionGroup<"opts">, HelpText<"Hexagon Linker experimental Options">;
def relax : Flag<["--"], "relax">,
                  HelpText<"Enable Hexagon Relaxation (default behavior)">,
                  Group<grp_hexagonexperimentalopts>;
defm relax_value
    : smDashOnlyEq<
          "relax", "relax_value",
          "Restrict relaxation to only certain sections">,
      MetaVarName<"<regex>">,
      Group<grp_hexagonexperimentalopts>;
