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
#include <list>
#include <string>

namespace llvm {
class Target;
class Triple;
} // namespace llvm

namespace eld {

class ELDTargetMachine;
class TargetRegistry;
class MCLinker;
class LinkerScript;
class LinkerConfig;
class Module;
class GNULDBackend;

/** \struct Target
 *  \brief Target collects target specific information
 */
struct Target {
  typedef unsigned int (*TripleMatchQualityFnTy)(const llvm::Triple &pTriple);

  typedef ELDTargetMachine *(*TargetMachineCtorTy)(const llvm::Target &,
                                                   const eld::Target &,
                                                   const std::string &);

  typedef bool (*EmulationFnTy)(LinkerScript &, LinkerConfig &);

  typedef GNULDBackend *(*GNULDBackendCtorTy)(Module &);

  Target();

  /// getName - get the target name
  const char *name() const { return Name; }

  unsigned int getTripleQuality(const llvm::Triple &pTriple) const;

  /// createTargetMachine - create target-specific TargetMachine
  ELDTargetMachine *createTargetMachine(const std::string &pTriple,
                                        const llvm::Target &pTarget) const;

  /// emulate - given MCLinker default values for the other aspects of the
  /// target system.
  bool emulate(LinkerScript &pScript, LinkerConfig &pConfig) const;

  /// createLDBackend - create target-specific LDBackend
  GNULDBackend *createLDBackend(Module &pModule) const;

  /// Name - The target name
  const char *Name;

  bool IsImplemented = false;

  TripleMatchQualityFnTy TripleMatchQualityFn;
  TargetMachineCtorTy TargetMachineCtorFn;
  EmulationFnTy EmulationFn;
  GNULDBackendCtorTy GNULDBackendCtorFn;
};

} // end namespace eld

#endif
