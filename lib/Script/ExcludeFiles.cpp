//===- ExcludeFiles.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "eld/Script/ExcludeFiles.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ExcludeFiles
//===----------------------------------------------------------------------===//
ExcludeFiles::ExcludeFiles() {}

ExcludeFiles::~ExcludeFiles() {}

void ExcludeFiles::pushBack(ExcludePattern *PPattern) {
  ExcludeFilesRule.push_back(PPattern);
}

// ExcludeFiles represents list of exclude patterns specified in EXCLUDE_FILE
// linker script directive. For example:
// OutSect : { EXCLUDE_FILE(*a.o *b.o) *(EXCLUDE_FILE(*c.o *d.o) *text*) }
//
// In the above example, EF1 may refer to the list [*a.o, *b.o] and
// EF2 may refer to the list [*c.o *d.o]
ExcludeFiles::ExcludeFiles(const ExcludeFiles *EF1, const ExcludeFiles *EF2) {
  if (EF1) {
    ExcludeFilesRule.insert(ExcludeFilesRule.end(),
                            EF1->ExcludeFilesRule.begin(),
                            EF1->ExcludeFilesRule.end());
  }
  if (EF2) {
    ExcludeFilesRule.insert(ExcludeFilesRule.end(),
                            EF2->ExcludeFilesRule.begin(),
                            EF2->ExcludeFilesRule.end());
  }
}
