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
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/TargetParser/Triple.h"
#include <string>
#include <vector>

namespace eld {

class TargetRegistry {
private:
  static std::vector<eld::Target *> TargetList;

public:
  static llvm::iterator_range<std::vector<eld::Target *>::iterator> targets() {
    return llvm::make_range(TargetList.begin(), TargetList.end());
  }

  static void RegisterTarget(Target &pTarget, llvm::StringRef Name,
                             uint16_t Machine, bool is64bit = false);

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

  static const eld::Target *
  lookupTarget(llvm::StringRef Name, llvm::Triple &pTriple, std::string &Error);

  // Find a target for a machine
  Target *lookupMachine(uint16_t Machine, bool is64bit) const;
};

struct RegisterTarget {
public:
  RegisterTarget(eld::Target &pTarget, llvm::StringRef Name, uint16_t Machine,
                 bool is64bit = false) {
    TargetRegistry::RegisterTarget(pTarget, Name, Machine, is64bit);
  }
};
} // end namespace eld

#endif
