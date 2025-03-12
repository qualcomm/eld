//===- StaticResolver.cpp--------------------------------------------------===//
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

#include "eld/SymbolResolver/StaticResolver.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Input/InputFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/ADT/CachedHashString.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Compiler.h"

using namespace eld;

namespace {
llvm::DenseSet<llvm::CachedHashStringRef> CommonBCSet;
}

//==========================
// StaticResolver
StaticResolver::~StaticResolver() {}

bool StaticResolver::resolve(ResolveInfo &pOld, const ResolveInfo &pNew,
                             bool &pOverride, LDSymbol::ValueType pValue,
                             LinkerConfig *Config, bool isPostLTOPhase) const {
  bool amITracing = Config->getPrinter()->traceSymbols();
  bool warnCommon = Config->options().warnCommon();

  /* The state table itself.
   * The first index is a link_row and the second index is a bfd_link_hash_type.
   *
   * Cs -> all rest kind of common (d_C, wd_C)
   * Is -> all kind of indirect
   */
  // clang-format off
  static const enum LinkAction link_action[LAST_ORD][LAST_ORD] =
  {
    /* new\old  U       w_U     d_U    wd_U   D      w_D    d_D    wd_D   C      w_C,   Cs,    */
    /* U    */ {NOACT,  UND,    UND,   UND,   NOACT, NOACT, DUND,  DUND,  NOACT, NOACT, NOACT },
    /* w_U  */ {NOACT,  NOACT,  NOACT, WEAK,  NOACT, NOACT, DUNDW, DUNDW, NOACT, NOACT, NOACT },
    /* d_U  */ {NOACT,  NOACT,  NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT },
    /* wd_U */ {NOACT,  NOACT,  NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT },
    /* D    */ {DEF,    DEF,    DEF,   DEF,   MDEF,  DEF,   DEF,   DEF,   CDEF,  CDEF,  CDEF  },
    /* w_D  */ {DEFW,   DEFW,   DEFW,  DEFW,  NOACT, NOACT, DEFW,  DEFW,  NOACT, NOACT, NOACT },
    /* d_D  */ {MDEFD,  MDEFD,  DEFD,  DEFD,  NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT },
    /* wd_D */ {MDEFWD, MDEFWD, DEFWD, DEFWD, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT },
    /* C    */ {COM,    COM,    COM,   COM,   CREF,  COM,   COM,   COM,   MBIG,  COM,   BIG   },
    /* w_C  */ {COM,    COM,    COM,   COM,   NOACT, NOACT, NOACT, NOACT, NOACT, NOACT, NOACT },
    /* Cs   */ {COM,    COM,    COM,   COM,   NOACT, NOACT, NOACT, NOACT, MBIG,  MBIG,  MBIG  },
  };
  // clang-format off


  // Special cases:
  // * when a dynamic defined symbol meets a dynamic weak defined symbol, act
  //   noting.
  // * when a undefined symbol meets a dynamic defined symbol, override by
  //   dynamic defined first, then recover back to undefined symbol later.
  // * when a dynamic defined symbol meets a undefined symbol or a weak
  //   undefined symbol, do not override, instead of marking.
  // * When a undefined symbol meets a dynamic defined symbol or a weak
  //   undefined symbol meets a dynamic defined symbol, should override.
  // * When a common symbol meets a weak common symbol, adjust the size of

  unsigned int row = getOrdinate(pNew);
  unsigned int col = getOrdinate(pOld);
  bool isOldBitCode = pOld.isBitCode();
  bool isNewBitCode = pNew.isBitCode();

  bool cycle = false;
  pOverride = false;
  ResolveInfo *old = &pOld;
  LinkAction action;

  do {
    cycle = false;
    action = link_action[row][col];

    switch (action) {
    case FAIL: { /* abort.  */
      Config->raise(diag::fail_sym_resolution) << __FILE__ << __LINE__;
      return false;
    }
    case NOACT: { /* no action.  */
      pOverride = false;
      if (!isPostLTOPhase) {
        if (!isNewBitCode && isOldBitCode && !pOld.isUndef())
          pOld.shouldPreserve(true);
        else if (pOld.isUndef() && !isNewBitCode && pNew.isUndef())
          // If there are two undefined symbols one in bitcode and the other
          // undefined symbol in ELF, we should preserve the symbol if there
          // is a definition in ELF.
          pOld.setInBitCode(false);
      }
      if (!old->isDyn())
        old->overrideVisibility(pNew);
      break;
    }
    case UND:   /* override by symbol undefined symbol.  */
    case WEAK:  /* override by symbol weak undefined.  */
    case DEF:   /* override by symbol defined.  */
    case DEFW:  /* override by symbol weak defined.  */
    case DEFD:  /* override by symbol dynamic defined.  */
    case DEFWD: /* override by symbol dynamic weak defined. */
    case COM: { /* override by symbol common defined.  */
      pOverride = true;
      if (!isPostLTOPhase) {
        // If the old symbol is weak and the new symbol is a defined symbol,
        // we should not preserve the old symbol. We should make sure that they
        // are not common symbols.
        if (!pOld.isCommon() && !pNew.isCommon()) {
          if (pOld.isWeak() && !pNew.isWeak() && isOldBitCode &&
              !isNewBitCode) {
            pOld.setInBitCode(false);
            pOld.shouldPreserve(false);
          }
        }
        if (!isOldBitCode && isNewBitCode && pOld.isUndef())
          pOld.shouldPreserve(true);
        if (!isNewBitCode && pOld.isBitCode() && !pOld.isUndef())
          pOld.shouldPreserve(true);
        if (isNewBitCode && !pNew.isUndef() && !isOldBitCode)
          pOld.shouldPreserve(true);
      }
      if (old->isPatchable() && !pNew.isPatchable())
        Config->raise(diag::error_patchable_override)
          << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
          << pNew.resolvedOrigin()->getInput()->decoratedPath();
      old->override(pNew);
      if (!old->isDyn())
        old->overrideVisibility(pNew);
      if (amITracing) {
        Config->raise(diag::adding_new_sym)
          << getOrdinateDesc(row) << pNew.name();
        Config->raise(diag::overriding_old_sym) << old->name()
          << getOrdinateDesc(col) << pNew.name();
      }
      break;
    }
    case MDEFD:    /* mark symbol dynamic defined.  */
    case MDEFWD: { /* mark symbol dynamic weak defined.  */
      if (amITracing)
        Config->raise(diag::marking_sym)
          << old->name() << getOrdinateDesc(row);

      if (pOld.visibility() != ResolveInfo::Default) {
        cycle = true;
        row = 0;
        col = 0;
        break;
      }
      bool isOldWeakUndef = pOld.isWeakUndef();
      old->override(pNew);
      if (isOldWeakUndef)
        old->setBinding(ResolveInfo::Weak);
      Config->raise(diag::mark_dynamic_defined) << old->name();
      pOverride = true;
      break;
    }
    case DUND:
    case DUNDW: {
      old->setVisibility(pNew.visibility());
      pOverride = false;
      if (amITracing) {
        Config->raise(diag::override_dyn_sym)
          << old->name() << old->resolvedOrigin()
          << getOrdinateDesc(row);
      }
      break;
    }
    case CREF: { /* Possibly warn about common reference to defined symbol.  */
      // A common symbol does not override a definition.
      if (warnCommon)
      Config->raise(diag::common_symbol_doesnot_override_definition)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      old->overrideVisibility(pNew);
      if (!isPostLTOPhase && !isNewBitCode && isOldBitCode)
        pOld.shouldPreserve(true);
      pOverride = false;
      break;
    }
    case CDEF: { /* redefine existing common symbol.  */
      // We've seen a common symbol and now we see a definition.  The
      // definition overrides.
      //
      // NOTE: m_Mesg uses 'name' instead of `name' for being compatible to GNU
      // ld.
      if (!isPostLTOPhase && ((isNewBitCode && !pNew.isUndef()) ||
                              (isOldBitCode ^ isNewBitCode))) {
        CommonBCSet.insert(llvm::CachedHashStringRef(old->name()));
        old->shouldPreserve(true);
      }
      if (warnCommon)
        Config->raise(diag::comm_override_by_definition)
              << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
              << pNew.resolvedOrigin()->getInput()->decoratedPath();
      old->override(pNew);
      old->overrideVisibility(pNew);
      pOverride = true;
      break;
    }
    case BIG: { /* override by symbol common using largest size.  */
      if (warnCommon)
      Config->raise(diag::comm_override_by_comm)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      if (old->size() < pNew.size()) {
        old->setSize(pNew.size());
        old->setInBitCode(pNew.isBitCode());
        old->setResolvedOrigin(pNew.resolvedOrigin());
      }
      if (!isPostLTOPhase && (isOldBitCode ^ isNewBitCode)) {
        CommonBCSet.insert(llvm::CachedHashStringRef(old->name()));
        old->shouldPreserve(true);
      }
      old->overrideAttributes(pNew);
      old->overrideVisibility(pNew);
      pOverride = true;
      if (amITracing)
        Config->raise(diag::override_common_sym) << old->name() << old->size();
      break;
    }
    case MBIG: { /* mark common symbol by larger size. */
      if (warnCommon)
      Config->raise(diag::comm_override_by_comm)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      if (old->size() < pNew.size()) {
        old->setSize(pNew.size());
        old->setInBitCode(pNew.isBitCode());
        old->setResolvedOrigin(pNew.resolvedOrigin());
        pOverride = true;
      } else
        pOverride = false;
      if (!isPostLTOPhase && (isOldBitCode ^ isNewBitCode)) {
        CommonBCSet.insert(llvm::CachedHashStringRef(old->name()));
        old->shouldPreserve(true);
      }
      old->overrideVisibility(pNew);
      if (amITracing)
        Config->raise(diag::mark_common_sym) << old->name() << old->size();
      break;
    }
    case MDEF: { /* multiple definition error.  */
      if (pOld.isDefine() && pNew.isDefine() && pOld.isAbsolute() &&
          pNew.isAbsolute() &&
          (pOld.desc() == pNew.desc() || pOld.desc() == ResolveInfo::NoType ||
           pNew.desc() == ResolveInfo::NoType)) {
        if (pOld.outSymbol()->value() == pValue) {
          pOverride = true;
          old->override(pNew);
          old->overrideVisibility(pNew);

          break;
        } else {
            Config->raise(diag::multiple_absolute_definitions)
                << pNew.name() << pOld.outSymbol()->value() << pValue;
          break;
        }
      }
        Config->raise(diag::multiple_definitions)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      break;
    }
    } // end of the big switch (action)
  } while (cycle);
  return true;
}
