//===- TargetRegistry.h----------------------------------------------------===//
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
#ifndef ELD_SUPPORT_TARGETREGISTRY_H
#define ELD_SUPPORT_TARGETREGISTRY_H
#include "eld/Support/Target.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/TargetParser/Triple.h"
#include <list>
#include <string>

namespace eld {

/** \class TargetRegistry
 *  \brief TargetRegistry is an object adapter of llvm::TargetRegistry
 */
class TargetRegistry {
public:
  typedef std::list<eld::Target *> TargetListTy;
  typedef TargetListTy::iterator iterator;

private:
  static TargetListTy s_TargetList;

public:
  static iterator begin() { return s_TargetList.begin(); }
  static iterator end() { return s_TargetList.end(); }

  static size_t size() { return s_TargetList.size(); }
  static bool empty() { return s_TargetList.empty(); }

  static llvm::iterator_range<TargetListTy::iterator> targets() {
    return llvm::make_range(s_TargetList.begin(), s_TargetList.end());
  }

  /// RegisterTarget - Register the given target. Attempts to register a
  /// target which has already been registered will be ignored.
  ///
  /// Clients are responsible for ensuring that registration doesn't occur
  /// while another thread is attempting to access the registry. Typically
  /// this is done by initializing all targets at program startup.
  static void RegisterTarget(Target &pTarget, const char *pName,
                             Target::TripleMatchQualityFnTy pQualityFn);

  /// RegisterTargetMachine - Register a TargetMachine implementation for the
  /// given target.
  static void RegisterTargetMachine(eld::Target &T,
                                    eld::Target::TargetMachineCtorTy Fn) {
    // Ignore duplicate registration.
    if (!T.TargetMachineCtorFn)
      T.TargetMachineCtorFn = Fn;
  }

  /// RegisterEmulation - Register a emulation function for the target.
  /// target.
  static void RegisterEmulation(eld::Target &T, eld::Target::EmulationFnTy Fn) {
    if (!T.EmulationFn)
      T.EmulationFn = Fn;
  }

  /// RegisterGNULDBackend - Register a GNULDBackend implementation for
  /// the given target.
  static void RegisterGNULDBackend(eld::Target &T,
                                   eld::Target::GNULDBackendCtorTy Fn) {
    if (!T.GNULDBackendCtorFn)
      T.GNULDBackendCtorFn = Fn;
  }

  static const eld::Target *lookupTarget(const std::string &pTriple,
                                         std::string &pError);

  static const eld::Target *lookupTarget(const std::string &pArchName,
                                         llvm::Triple &pTriple,
                                         std::string &Error);
};

/// RegisterTarget - Helper function for registering a target, for use in the
/// target's initialization function. Usage:
///
/// Target TheFooTarget; // The global target instance.
///
template <llvm::Triple::ArchType TargetArchType = llvm::Triple::UnknownArch>
struct RegisterTarget {
public:
  RegisterTarget(eld::Target &pTarget, const char *pName) {
    // if we've registered one, then return immediately.
    TargetRegistry::iterator target, ie = TargetRegistry::end();
    for (target = TargetRegistry::begin(); target != ie; ++target) {
      if (0 == strcmp((*target)->name(), pName))
        return;
    }

    TargetRegistry::RegisterTarget(pTarget, pName, &getTripleMatchQuality);
  }

  static unsigned int getTripleMatchQuality(const llvm::Triple &pTriple) {
    if (pTriple.getArch() == TargetArchType)
      return 20;
    return 0;
  }
};

/// RegisterTargetMachine - Helper template for registering a target machine
/// implementation, for use in the target machine initialization
/// function. Usage:
///
/// extern "C" void ELDInitializeFooTarget() {
///   extern eld::Target TheFooTarget;
///   RegisterTargetMachine<eld::FooTargetMachine> X(TheFooTarget);
/// }
template <class TargetMachineImpl> struct RegisterTargetMachine {
  RegisterTargetMachine(eld::Target &T) {
    TargetRegistry::RegisterTargetMachine(T, &Allocator);
  }

private:
  static ELDTargetMachine *Allocator(const llvm::Target &pLLVMTarget,
                                     const eld::Target &pELDTarget,
                                     const std::string &pTriple) {
    return new TargetMachineImpl(pLLVMTarget, pELDTarget, pTriple);
  }
};

} // end namespace eld

#endif
