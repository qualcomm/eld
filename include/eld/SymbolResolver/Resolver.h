//===- Resolver.h----------------------------------------------------------===//
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

#ifndef ELD_SYMBOLRESOLVER_RESOLVER_H
#define ELD_SYMBOLRESOLVER_RESOLVER_H
#include "eld/SymbolResolver/LDSymbol.h"
#include <string>
#include <utility>

namespace eld {

class ResolveInfo;
class NamePool;
class LinkerConfig;

/** \class Resolver
 *  \brief Resolver binds a symbol reference from one file to a symbol
 *   definition of another file.
 *
 *  Resolver seals up the algorithm of symbol resolution. The resolution of
 *  two symbols depends on their type, binding and whether it is belonging to
 *  a shared object.
 */
class Resolver {
public:
  enum Action { Success, Warning, Abort, LastAction };

  /** \class Resolver::Result
   *  \brief the result of symbol resolution
   *   - info, the pointer to overrided info
   *   - existent, if true, the info is existent
   *   - overriden, if true, the info is being overriden.
   */
  struct Result {
    ResolveInfo *Info = nullptr;
    bool Existent = false;
    bool Overriden = false;
  };

public:
  virtual ~Resolver();

  /// shouldOverride - Can resolver override the symbol pOld by the symbol pNew?
  /// @return the action should be taken.
  /// @param pOld the symbol which may be overridden.
  /// @param pNew the symbol which is used to replace pOld
  virtual bool resolve(ResolveInfo &OldSymbol, const ResolveInfo &NewSymbol,
                       bool &CurOverride, LDSymbol::ValueType Value,
                       LinkerConfig *Config, bool IsPostLtoPhase) const = 0;
};

} // namespace eld

#endif
