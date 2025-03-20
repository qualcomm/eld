//===- Linker.h------------------------------------------------------------===//
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
#ifndef ELD_CORE_LINKER_H
#define ELD_CORE_LINKER_H

#include "eld/Core/Module.h"
#include "eld/Input/InputAction.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/Timer.h"
#include <memory>
#include <string>
#include <vector>

namespace eld {

class Module;
class LinkerConfig;
class LinkerScript;
class ProgressBar;

struct Target;
class GNULDBackend;

class IRBuilder;
class ObjectLinker;

/** \class Linker
 *  \brief Linker is a modular linker.
 */
class Linker {
public:
  enum UnresolvedPolicy {
    NotSet = 0x0,
    IgnoreAll = 0x1,
    ReportAll = 0x2,
    IgnoreInObjectFiles = 0x4,
    IgnoreInSharedLibrary = 0x8,
  };

  Linker(Module &Module, LinkerConfig &Config);

  ~Linker();

  // Prepare all the input files and various data structures for the link to
  // proceed further.
  bool prepare(std::vector<InputAction *> &Actions, const eld::Target *Target);

  // Do the actual linking process
  bool link();

  ObjectLinker *getObjLinker() const { return ObjLinker; }

  eld::IRBuilder *getIRBuilder() const { return IR; }

  bool shouldIgnoreUndefine(bool IsDyn) {
    return (!UnresolvedSymbolPolicy ||
            (!(UnresolvedSymbolPolicy & UnresolvedPolicy::ReportAll) &&
             ((UnresolvedSymbolPolicy & UnresolvedPolicy::IgnoreAll) ||
              (!IsDyn && (UnresolvedSymbolPolicy &
                          UnresolvedPolicy::IgnoreInObjectFiles)) ||
              (IsDyn && (UnresolvedSymbolPolicy &
                         UnresolvedPolicy::IgnoreInSharedLibrary)))));
  }

  void setUnresolvePolicy(llvm::StringRef Option);

  GNULDBackend *getBackend() const { return Backend; }

  ObjectLinker *getObjectLinker() const {
    ASSERT(ObjLinker, "m_pObjLinker must not be null!");
    return ObjLinker;
  }

  void printLayout();

  void unloadPlugins();

private:
  bool initBackend(const eld::Target *PTarget);

  bool initEmulator(LinkerScript &CurScript, const eld::Target *PTarget);

  bool activateInputs(std::vector<InputAction *> &Actions);

  bool initializeInputTree(std::vector<InputAction *> &Actions);

  bool emulate();

  bool normalize();

  bool resolve();

  bool loadNonUniversalPlugins();

  bool layout();

  bool emit();

  bool reset();

  bool verifyLinkerScript();

  /// Record common symbols information using the layout printer.
  /// \note Bitcode common symbol information is not recorded.
  void recordCommonSymbols();

  /// Record common symbol R information using the layout printer.
  void recordCommonSymbol(const ResolveInfo *R);

  void reportUnknownOptions() const;

private:
  Module *ThisModule;
  LinkerConfig *ThisConfig;
  GNULDBackend *Backend;
  ObjectLinker *ObjLinker;
  eld::IRBuilder *IR;
  ProgressBar *LinkerProgress;
  llvm::Timer *LinkTime;
  llvm::Timer *TimingSectionTimer;
  uint32_t UnresolvedSymbolPolicy;
  uint64_t BeginningOfTime;
};

} // namespace eld

#endif
