//===- AArch64.h-----------------------------------------------------------===//
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
#ifndef TARGET_AARCH64_AARCH64_H
#define TARGET_AARCH64_AARCH64_H
#include <string>

namespace llvm {
class Target;
} // namespace llvm

namespace eld {

struct Target;
class GNULDBackend;

extern eld::Target TheAArch64Target;

GNULDBackend *createAArch64LDBackend(const llvm::Target &, const std::string &);

} // namespace eld

#endif
