//===- ELDDirectory.cpp----------------------------------------------------===//
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

#include "eld/Input/ELDDirectory.h"
#include "eld/Support/FileSystem.h"

using namespace eld;
using namespace eld::sys::fs;

//===----------------------------------------------------------------------===//
// ELDDirectory
//===----------------------------------------------------------------------===//
ELDDirectory::ELDDirectory(llvm::StringRef PName, std::string SysRoot)
    : Name(PName), IsFound(false) {
  if (PName.starts_with("=/")) {
    // If a search directory begins with "=", "=" is replaced
    // with the sysroot path.
    Name.assign(SysRoot);
    Name.append(PName.substr(1).str());
    BInSysroot = true;
  }
}

ELDDirectory::~ELDDirectory() {}
