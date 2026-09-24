//===- X86.h---------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_H
#define ELD_TARGET_X86_H

namespace eld {

struct Target;
class GNULDBackend;
class LinkerConfig;
class LinkerScript;
class Module;

extern Target Thex86_64Target;

bool emulatex86_64LD(LinkerScript &, LinkerConfig &);
GNULDBackend *createx86_64LDBackend(Module &);

} // namespace eld

#endif
