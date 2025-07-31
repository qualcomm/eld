//===- Target.h------------------------------------------------------------===//
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
#ifndef ELD_SUPPORT_TARGET_H
#define ELD_SUPPORT_TARGET_H
#include "llvm/TargetParser/Triple.h"
#include <string>

namespace eld {

class TargetRegistry;
class LinkerScript;
class LinkerConfig;
class Module;
class GNULDBackend;

/** \struct Target
 *  \brief Target collects target specific information
 */
struct Target {
  typedef bool (*EmulationFnTy)(LinkerScript &, LinkerConfig &);

  typedef GNULDBackend *(*GNULDBackendCtorTy)(Module &);

  Target();

  /// getName - get the target name
  llvm::StringRef name() const { return Name; }

  /// emulate - given MCLinker default values for the other aspects of the
  /// target system.
  bool emulate(LinkerScript &pScript, LinkerConfig &pConfig) const;

  /// createLDBackend - create target-specific LDBackend
  GNULDBackend *createLDBackend(Module &pModule) const;

  /// Name - The target name
  llvm::StringRef Name;

  // Machine
  uint16_t Machine = 0;

  // Is64bit
  bool Is64bit = false;

  EmulationFnTy EmulationFn = nullptr;
  GNULDBackendCtorTy GNULDBackendCtorFn = nullptr;
};

} // end namespace eld

#endif
