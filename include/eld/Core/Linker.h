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

  Linker(Module &module, LinkerConfig &config);

  ~Linker();

  // Prepare all the input files and various data structures for the link to
  // proceed further.
  bool prepare(std::vector<InputAction *> &actions, const eld::Target *target);

  // Do the actual linking process
  bool link();

  ObjectLinker *getObjLinker() const { return m_pObjLinker; }

  eld::IRBuilder *getIRBuilder() const { return m_pBuilder; }

  bool shouldIgnoreUndefine(bool isDyn) {
    return (!UnresolvedSymbolPolicy ||
            (!(UnresolvedSymbolPolicy & UnresolvedPolicy::ReportAll) &&
             ((UnresolvedSymbolPolicy & UnresolvedPolicy::IgnoreAll) ||
              (!isDyn && (UnresolvedSymbolPolicy &
                          UnresolvedPolicy::IgnoreInObjectFiles)) ||
              (isDyn && (UnresolvedSymbolPolicy &
                         UnresolvedPolicy::IgnoreInSharedLibrary)))));
  }

  void setUnresolvePolicy(llvm::StringRef Option);

  GNULDBackend *getBackend() const { return m_pBackend; }

  ObjectLinker *getObjectLinker() const {
    ASSERT(m_pObjLinker, "m_pObjLinker must not be null!");
    return m_pObjLinker;
  }

  void printLayout();

  void unloadPlugins();

private:
  bool initBackend(const eld::Target *pTarget);

  bool initEmulator(LinkerScript &pScript, const eld::Target *pTarget);

  bool activateInputs(std::vector<InputAction *> &actions);

  bool initializeInputTree(std::vector<InputAction *> &actions);

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
  Module *m_pModule;
  LinkerConfig *m_pConfig;
  GNULDBackend *m_pBackend;
  ObjectLinker *m_pObjLinker;
  eld::IRBuilder *m_pBuilder;
  ProgressBar *m_pProgressBar;
  llvm::Timer *m_LinkTime;
  llvm::Timer *m_TimingSectionTimer;
  uint32_t UnresolvedSymbolPolicy;
  uint64_t m_BeginningOfTime;
};

} // namespace eld

#endif
