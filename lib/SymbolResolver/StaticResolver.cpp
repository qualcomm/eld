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
} // namespace

//==========================
// StaticResolver
StaticResolver::~StaticResolver() {}

bool StaticResolver::resolve(ResolveInfo &pOld, const ResolveInfo &pNew,
                             bool &CurOverride, LDSymbol::ValueType pValue,
                             LinkerConfig *Config, bool IsPostLtoPhase) const {
  bool AmITracing = Config->getPrinter()->traceSymbols();
  bool WarnCommon = Config->options().warnCommon();

  /* The state table itself.
   * The first index is a link_row and the second index is a bfd_link_hash_type.
   *
   * Cs -> all rest kind of common (d_C, wd_C)
   * Is -> all kind of indirect
   */
  // clang-format off
  static const enum LinkAction LinkAction[LAST_ORD][LAST_ORD] =
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

  unsigned int Row = getOrdinate(pNew);
  unsigned int Col = getOrdinate(pOld);
  bool IsOldBitCode = pOld.isBitCode();
  bool IsNewBitCode = pNew.isBitCode();

  bool Cycle = false;
  CurOverride = false;
  ResolveInfo *Old = &pOld;
  enum LinkAction Action;

  do {
    Cycle = false;
    Action = LinkAction[Row][Col];

    switch (Action) {
    case FAIL: { /* abort.  */
      Config->raise(Diag::fail_sym_resolution) << __FILE__ << __LINE__;
      return false;
    }
    case NOACT: { /* no action.  */
      CurOverride = false;
      if (!IsPostLtoPhase) {
        if (!IsNewBitCode && IsOldBitCode && !pOld.isUndef())
          pOld.shouldPreserve(true);
        else if (pOld.isUndef() && !IsNewBitCode && pNew.isUndef())
          // If there are two undefined symbols one in bitcode and the other
          // undefined symbol in ELF, we should preserve the symbol if there
          // is a definition in ELF.
          pOld.setInBitCode(false);
      }
      if (!Old->isDyn())
        Old->overrideVisibility(pNew);
      if (Old->canBePreemptible() && pNew.isDyn() && pNew.canBePreemptible())
        Old->setExportToDyn();
      break;
    }
    case UND:   /* override by symbol undefined symbol.  */
    case WEAK:  /* override by symbol weak undefined.  */
    case DEF:   /* override by symbol defined.  */
    case DEFW:  /* override by symbol weak defined.  */
    case DEFD:  /* override by symbol dynamic defined.  */
    case DEFWD: /* override by symbol dynamic weak defined. */
    case COM: { /* override by symbol common defined.  */
      CurOverride = true;
      if (!IsPostLtoPhase) {
        // If the old symbol is weak and the new symbol is a defined symbol,
        // we should not preserve the old symbol. We should make sure that they
        // are not common symbols.
        if (!pOld.isCommon() && !pNew.isCommon()) {
          if (pOld.isWeak() && !pNew.isWeak() && IsOldBitCode &&
              !IsNewBitCode) {
            pOld.setInBitCode(false);
            pOld.shouldPreserve(false);
          }
        }
        if (!IsOldBitCode && IsNewBitCode && pOld.isUndef())
          pOld.shouldPreserve(true);
        if (!IsNewBitCode && pOld.isBitCode() && !pOld.isUndef())
          pOld.shouldPreserve(true);
        if (IsNewBitCode && !pNew.isUndef() && !IsOldBitCode)
          pOld.shouldPreserve(true);
      }
      if (Old->isPatchable() && !pNew.isPatchable())
        Config->raise(Diag::error_patchable_override)
          << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
          << pNew.resolvedOrigin()->getInput()->decoratedPath();
      Old->override(pNew);
      if (!Old->isDyn())
        Old->overrideVisibility(pNew);
      if (AmITracing) {
        Config->raise(Diag::adding_new_sym)
          << getOrdinateDesc(Row) << pNew.name();
        Config->raise(Diag::overriding_old_sym) << Old->name()
          << getOrdinateDesc(Col) << pNew.name();
      }
      break;
    }
    case MDEFD:    /* mark symbol dynamic defined.  */
    case MDEFWD: { /* mark symbol dynamic weak defined.  */
      if (AmITracing)
        Config->raise(Diag::marking_sym)
          << Old->name() << getOrdinateDesc(Row);

      if (pOld.visibility() != ResolveInfo::Default) {
        Cycle = true;
        Row = 0;
        Col = 0;
        break;
      }
      bool IsOldWeakUndef = pOld.isWeakUndef();
      Old->override(pNew);
      if (IsOldWeakUndef)
        Old->setBinding(ResolveInfo::Weak);
      Config->raise(Diag::mark_dynamic_defined) << Old->name();
      CurOverride = true;
      break;
    }
    case DUND:
    case DUNDW: {
      Old->setVisibility(pNew.visibility());
      CurOverride = false;
      if (AmITracing) {
        Config->raise(Diag::override_dyn_sym)
          << Old->name() << Old->resolvedOrigin()
          << getOrdinateDesc(Row);
      }
      break;
    }
    case CREF: { /* Possibly warn about common reference to defined symbol.  */
      // A common symbol does not override a definition.
      if (WarnCommon)
      Config->raise(Diag::common_symbol_doesnot_override_definition)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      Old->overrideVisibility(pNew);
      if (!IsPostLtoPhase && !IsNewBitCode && IsOldBitCode)
        pOld.shouldPreserve(true);
      CurOverride = false;
      break;
    }
    case CDEF: { /* redefine existing common symbol.  */
      // We've seen a common symbol and now we see a definition.  The
      // definition overrides.
      //
      // NOTE: m_Mesg uses 'name' instead of `name' for being compatible to GNU
      // ld.
      if (!IsPostLtoPhase && ((IsNewBitCode && !pNew.isUndef()) ||
                              (IsOldBitCode ^ IsNewBitCode))) {
        CommonBCSet.insert(llvm::CachedHashStringRef(Old->name()));
        Old->shouldPreserve(true);
      }
      if (WarnCommon)
        Config->raise(Diag::comm_override_by_definition)
              << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
              << pNew.resolvedOrigin()->getInput()->decoratedPath();
      Old->override(pNew);
      Old->overrideVisibility(pNew);
      CurOverride = true;
      break;
    }
    case BIG: { /* override by symbol common using largest size.  */
      if (WarnCommon)
      Config->raise(Diag::comm_override_by_comm)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      if (Old->size() < pNew.size()) {
        Old->setSize(pNew.size());
        Old->setInBitCode(pNew.isBitCode());
        Old->setResolvedOrigin(pNew.resolvedOrigin());
      }
      if (!IsPostLtoPhase && (IsOldBitCode ^ IsNewBitCode)) {
        CommonBCSet.insert(llvm::CachedHashStringRef(Old->name()));
        Old->shouldPreserve(true);
      }
      Old->overrideAttributes(pNew);
      Old->overrideVisibility(pNew);
      CurOverride = true;
      if (AmITracing)
        Config->raise(Diag::override_common_sym) << Old->name() << Old->size();
      break;
    }
    case MBIG: { /* mark common symbol by larger size. */
      if (WarnCommon)
      Config->raise(Diag::comm_override_by_comm)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      if (Old->size() < pNew.size()) {
        Old->setSize(pNew.size());
        Old->setInBitCode(pNew.isBitCode());
        Old->setResolvedOrigin(pNew.resolvedOrigin());
        CurOverride = true;
      } else
        CurOverride = false;
      if (!IsPostLtoPhase && (IsOldBitCode ^ IsNewBitCode)) {
        CommonBCSet.insert(llvm::CachedHashStringRef(Old->name()));
        Old->shouldPreserve(true);
      }
      Old->overrideVisibility(pNew);
      if (AmITracing)
        Config->raise(Diag::mark_common_sym) << Old->name() << Old->size();
      break;
    }
    case MDEF: { /* multiple definition error.  */
      if (pOld.isDefine() && pNew.isDefine() && pOld.isAbsolute() &&
          pNew.isAbsolute() &&
          (pOld.desc() == pNew.desc() || pOld.desc() == ResolveInfo::NoType ||
           pNew.desc() == ResolveInfo::NoType)) {
        if (pOld.outSymbol()->value() == pValue) {
          CurOverride = true;
          Old->override(pNew);
          Old->overrideVisibility(pNew);

          break;
        }             Config->raise(Diag::multiple_absolute_definitions)
                << pNew.name() << pOld.outSymbol()->value() << pValue;
          break;

      }
        Config->raise(Diag::multiple_definitions)
            << pOld.name() << pOld.resolvedOrigin()->getInput()->decoratedPath()
            << pNew.resolvedOrigin()->getInput()->decoratedPath();
      break;
    }
    } // end of the big switch (action)
  } while (Cycle);
  return true;
}
