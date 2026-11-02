//===- x86_64LinkerOptions.td ---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

include "GnuLinkerOptions.td"

def grp_x86_64_linker : OptionGroup<"opts">, HelpText<"X86-64 Linker Options">;

