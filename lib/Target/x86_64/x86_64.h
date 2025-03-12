//===- x86_64.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_TARGET_X86_64_H
#define ELD_TARGET_X86_64_H
#include <string>

namespace llvm {
class Target;
} // namespace llvm

namespace eld {

struct Target;
class GNULDBackend;

extern eld::Target Thex86_64Target;

GNULDBackend *createx86_64LDBackend(const llvm::Target &, const std::string &);

} // namespace eld

#endif
