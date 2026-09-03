//===- eld.cpp-------------------------------------------------------------===//
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
//
// This executable is a forwarder into the LinkerWrapper library. Nothing
// here should call into LLVM, in order to avoid duplicate copies of LLVM
// globals between libLW.so and ld.eld.
//
//===----------------------------------------------------------------------===//
#include "eld/Support/Defines.h"

class DLL_A_EXPORT Driver {
public:
  static int main(int Argc, const char **Argv);
};

int main(int Argc, const char **Argv) { return Driver::main(Argc, Argv); }
