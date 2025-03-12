//===- ELFEmulation.h------------------------------------------------------===//
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
#ifndef ELD_TARGET_ELFEMULATION_H
#define ELD_TARGET_ELFEMULATION_H

namespace eld {

class LinkerConfig;
class LinkerScript;

bool ELDEmulateELF(LinkerScript &pScript, LinkerConfig &pConfig);

} // namespace eld

#endif
