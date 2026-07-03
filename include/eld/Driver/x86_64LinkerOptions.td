//===- x86_64LinkerOptions.td ---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "GnuLinkerOptions.td"

def grp_x86_64_linker : OptionGroup<"opts">, HelpText<"X86-64 Linker Options">;

def relax : Flag<["--"], "relax">,
            HelpText<"Enable GOTPCRELX relaxation (GOT-indirect to PC-relative)">,
            Group<grp_x86_64_linker>;

def no_relax : Flag<["--"], "no-relax">,
               HelpText<"Disable GOTPCRELX relaxation">,
               Group<grp_x86_64_linker>;
