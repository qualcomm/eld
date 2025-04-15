//===- ARMRelocator.cpp----------------------------------------------------===//
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

#include "ARMRelocator.h"
#include "ARMRelocationFunctions.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Object/ObjectBuilder.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/IRBuilder.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/MathExtras.h"

// ApplyReloc Mutex.
namespace {
std::mutex m_ApplyRelocMutex;
}

using namespace eld;

//=========================================//
// Relocation helper function              //
//=========================================//
static Relocator::DWord getThumbBit(ARMRelocator &pParent, Relocation &pReloc,
                                    bool IsJump) {
  if (!pReloc.symInfo())
    return 0;
  // Set thumb bit if
  // - symbol has type of STT_FUNC, is defined and with bit 0 of its value set
  //
  // If the relocation is a jump, pretend the target is STT_FUNC even if it
  // isn't marked that way; otherwise, we would just throw away bit 0.
  Relocator::DWord thumbBit =
      ((!pReloc.symInfo()->isUndef() || pReloc.symInfo()->isDyn()) &&
       (pReloc.symInfo()->type() == ResolveInfo::Function || IsJump) &&
       ((pParent.getSymValue(&pReloc) & 0x1) != 0))
          ? 1
          : 0;
  return thumbBit;
}

static uint64_t helper_bit_select(uint64_t pA, uint64_t pB, uint64_t pMask) {
  return (pA & ~pMask) | (pB & pMask);
}

// Strip LSB (THUMB bit) if "S" is a THUMB target.
static inline void helper_clear_thumb_bit(Relocator::Address &pValue) {
  pValue &= (~0x1);
}

// Get an relocation entry in .rel.dyn and set its type to pType,
// its FragmentRef to pReloc->targetFrag() and its ResolveInfo to
// pReloc->symInfo()
static Relocation *helper_DynRel_init(ELFObjectFile *Obj, Relocation *R,
                                      ResolveInfo *pSym, Fragment *F,
                                      uint32_t pOffset, Relocator::Type pType,
                                      ARMGNULDBackend &B) {
  Relocation *rel_entry = Obj->getRelaDyn()->createOneReloc();
  rel_entry->setType(pType);
  rel_entry->setTargetRef(make<FragmentRef>(*F, pOffset));
  rel_entry->setSymInfo(pSym);
  if (R)
    rel_entry->setAddend(R->addend());

  // This is one insane thing, that we need to do. scanRelocations is called
  // rightly before merge sections, so any strings that are merged need to be
  // updated after merge is done to get the right symbol value. Lets record the
  // fact that we created a relative relocation for a relocation that may be
  // pointing to a merge string.
  if (R && (pType == llvm::ELF::R_ARM_RELATIVE ||
            pType == llvm::ELF::R_ARM_IRELATIVE)) {
    B.recordRelativeReloc(rel_entry, R);
  }
  return rel_entry;
}

static ARMGOT *CreateGOT(ELFObjectFile *Obj, Relocation &pReloc, bool pHasRel,
                         ARMGNULDBackend &B, bool isExec) {
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  ARMGOT *G = B.createGOT(GOT::Regular, Obj, rsym);

  if (!pHasRel) {
    G->setValueType(GOT::SymbolValue);
    return G;
  }

  // If the symbol is not preemptible and we are not building an executable,
  // then try to use a relative reloc. We use a relative reloc if the symbol is
  // hidden otherwise.
  bool useRelative =
      (rsym->isHidden() || (!isExec && !B.isSymbolPreemptible(*rsym)));

  helper_DynRel_init(
      Obj, &pReloc, rsym, G, 0x0,
      useRelative ? llvm::ELF::R_ARM_RELATIVE : llvm::ELF::R_ARM_GLOB_DAT, B);
  if (useRelative)
    G->setValueType(GOT::SymbolValue);
  return G;
}

static Relocator::DWord
helper_extract_movw_movt_addend(Relocator::DWord pTarget) {
  // imm16: [19-16][11-0]
  return llvm::SignExtend64((((pTarget >> 4)) & 0xf000U) | (pTarget & 0xfffU),
                            16);
}

static Relocator::DWord
helper_insert_val_movw_movt_inst(Relocator::DWord pTarget,
                                 Relocator::DWord pImm) {
  // imm16: [19-16][11-0]
  pTarget &= 0xfff0f000U;
  pTarget |= pImm & 0x0fffU;
  pTarget |= (pImm & 0xf000U) << 4;
  return pTarget;
}

static Relocator::DWord
helper_extract_thumb_movw_movt_addend(Relocator::DWord pValue) {
  // imm16: [19-16][26][14-12][7-0]
  return llvm::SignExtend64((((pValue >> 4) & 0xf000U) |
                             ((pValue >> 15) & 0x0800U) |
                             ((pValue >> 4) & 0x0700U) | (pValue & 0x00ffU)),
                            16);
}

static Relocator::DWord
helper_insert_val_thumb_movw_movt_inst(Relocator::DWord pValue,
                                       Relocator::DWord pImm) {
  // imm16: [19-16][26][14-12][7-0]
  pValue &= 0xfbf08f00U;
  pValue |= (pImm & 0xf000U) << 4;
  pValue |= (pImm & 0x0800U) << 15;
  pValue |= (pImm & 0x0700U) << 4;
  pValue |= (pImm & 0x00ffU);
  return pValue;
}

static Relocator::DWord
helper_thumb32_branch_offset(Relocator::DWord pUpper16,
                             Relocator::DWord pLower16) {
  Relocator::DWord s = (pUpper16 & (1U << 10)) >> 10, // 26 bit
      u = pUpper16 & 0x3ffU,                          // 25-16
      l = pLower16 & 0x7ffU,                          // 10-0
      j1 = (pLower16 & (1U << 13)) >> 13,             // 13
      j2 = (pLower16 & (1U << 11)) >> 11;             // 11
  Relocator::DWord i1 = j1 ^ s ? 0 : 1, i2 = j2 ^ s ? 0 : 1;

  // [31-25][24][23][22][21-12][11-1][0]
  //      0   s  i1  i2      u     l  0
  return llvm::SignExtend64(
      (s << 24) | (i1 << 23) | (i2 << 22) | (u << 12) | (l << 1), 25);
}

static Relocator::DWord helper_thumb32_branch_upper(Relocator::DWord pUpper16,
                                                    Relocator::DWord pOffset) {
  uint32_t sign = ((pOffset & 0x80000000U) >> 31);
  return (pUpper16 & ~0x7ffU) | ((pOffset >> 12) & 0x3ffU) | (sign << 10);
}

static Relocator::DWord helper_thumb32_branch_lower(Relocator::DWord pLower16,
                                                    Relocator::DWord pOffset) {
  uint32_t sign = ((pOffset & 0x80000000U) >> 31);
  return ((pLower16 & ~0x2fffU) | ((((pOffset >> 23) & 1) ^ !sign) << 13) |
          ((((pOffset >> 22) & 1) ^ !sign) << 11) | ((pOffset >> 1) & 0x7ffU));
}

static Relocator::DWord
helper_thumb32_cond_branch_offset(Relocator::DWord pUpper16,
                                  Relocator::DWord pLower16) {
  uint32_t s = (pUpper16 & 0x0400U) >> 10;
  uint32_t j1 = (pLower16 & 0x2000U) >> 13;
  uint32_t j2 = (pLower16 & 0x0800U) >> 11;
  uint32_t lower = (pLower16 & 0x07ffU);
  uint32_t upper = (s << 8) | (j2 << 7) | (j1 << 6) | (pUpper16 & 0x003fU);
  return llvm::SignExtend64((upper << 12) | (lower << 1), 21);
}

static Relocator::DWord
helper_thumb32_cond_branch_upper(Relocator::DWord pUpper16,
                                 Relocator::DWord pOffset) {
  uint32_t sign = ((pOffset & 0x80000000U) >> 31);
  return (pUpper16 & 0xfbc0U) | (sign << 10) | ((pOffset & 0x0003f000U) >> 12);
}

static Relocator::DWord
helper_thumb32_cond_branch_lower(Relocator::DWord pLower16,
                                 Relocator::DWord pOffset) {
  uint32_t j2 = (pOffset & 0x00080000U) >> 19;
  uint32_t j1 = (pOffset & 0x00040000U) >> 18;
  uint32_t lo = (pOffset & 0x00000ffeU) >> 1;
  return (pLower16 & 0xd000U) | (j1 << 13) | (j2 << 11) | lo;
}

// Return true if overflow
static bool helper_check_signed_overflow(Relocator::DWord pValue,
                                         unsigned bits) {
  int32_t signed_val = static_cast<int32_t>(pValue);
  int32_t max = (1 << (bits - 1)) - 1;
  int32_t min = -(1 << (bits - 1));
  if (signed_val > max || signed_val < min) {
    return true;
  } else {
    return false;
  }
}

ARMGOT *ARMRelocator::getTLSModuleID(ResolveInfo *R, bool isStatic) {
  static ARMGOT *G = nullptr;
  if (G != nullptr) {
    m_Target.recordGOT(R, G);
    return G;
  }

  G = m_Target.createGOT(GOT::TLS_LD, nullptr, nullptr);

  if (!isStatic)
    helper_DynRel_init(m_Target.getDynamicSectionHeadersInputFile(), nullptr,
                       nullptr, G, 0x0, llvm::ELF::R_ARM_TLS_DTPMOD32,
                       m_Target);

  m_Target.recordGOT(R, G);
  return G;
}

//===--------------------------------------------------------------------===//
// Relocation Functions and Tables
//===--------------------------------------------------------------------===//
DECL_ARM_APPLY_RELOC_FUNCS

/// the prototype of applying function
typedef Relocator::Result (*ApplyFunctionType)(Relocation &pReloc,
                                               ARMRelocator &pParent);

// the table entry of applying functions
struct ApplyFunctionTriple {
  ApplyFunctionType func;
  unsigned int type;
  const char *name;
};

// declare the table of applying functions
static const ApplyFunctionTriple ApplyFunctions[] = {
    DECL_ARM_APPLY_RELOC_FUNC_PTRS};

//===--------------------------------------------------------------------===//
// ARMRelocator
//===--------------------------------------------------------------------===//
ARMRelocator::ARMRelocator(ARMGNULDBackend &pParent, LinkerConfig &pConfig,
                           Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {}

ARMRelocator::~ARMRelocator() {}

bool ARMRelocator::isInvalidReloc(Relocation &pReloc) const {
  if (!config().isCodeIndep())
    return false;
  switch (pReloc.type()) {
  case llvm::ELF::R_ARM_ABS16:
  case llvm::ELF::R_ARM_ABS12:
  case llvm::ELF::R_ARM_THM_ABS5:
  case llvm::ELF::R_ARM_ABS8:
  case llvm::ELF::R_ARM_BASE_ABS:
  case llvm::ELF::R_ARM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_MOVT_ABS:
  case llvm::ELF::R_ARM_THM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_THM_MOVT_ABS:
  case llvm::ELF::R_ARM_TLS_LE32:
  case llvm::ELF::R_ARM_TLS_LE12:
  case llvm::ELF::R_ARM_TLS_IE12GP:
    return true;
  default:
    break;
  }
  return false;
}

Relocator::Result ARMRelocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();
  if (type > 133) { // 131-255 doesn't noted in ARM spec
    return Relocator::Unknown;
  }

  ResolveInfo *symInfo = pRelocation.symInfo();

  if (symInfo) {
    LDSymbol *outSymbol = symInfo->outSymbol();
    if (outSymbol && outSymbol->hasFragRef()) {
      ELFSection *S = outSymbol->fragRef()->frag()->getOwningSection();
      if (S->isDiscard() ||
          (S->getOutputSection() && S->getOutputSection()->isDiscard())) {
        std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
        issueUndefRef(pRelocation, *S->getInputFile(), S);
        return Relocator::OK;
      }
    }
  }

  return ApplyFunctions[type].func(pRelocation, *this);
}

const char *ARMRelocator::getName(Relocator::Type pType) const {
  return ApplyFunctions[pType].name;
}

uint32_t ARMRelocator::getNumRelocs() const { return ARM_MAXRELOCS; }

Relocator::Size ARMRelocator::getSize(Relocation::Type pType) const {
  return 32;
}

/// checkValidReloc - When we attempt to generate a dynamic relocation for
/// output file, check if the relocation is supported by dynamic linker.
bool ARMRelocator::checkValidReloc(Relocation &pReloc) const {
  // If not PIC object, no relocation type is invalid
  if (!config().isCodeIndep())
    return true;

  switch (pReloc.type()) {
  case llvm::ELF::R_ARM_RELATIVE:
  case llvm::ELF::R_ARM_COPY:
  case llvm::ELF::R_ARM_GLOB_DAT:
  case llvm::ELF::R_ARM_JUMP_SLOT:
  case llvm::ELF::R_ARM_ABS32:
  case llvm::ELF::R_ARM_ABS32_NOI:
  case llvm::ELF::R_ARM_PC24:
  case llvm::ELF::R_ARM_TLS_DTPMOD32:
  case llvm::ELF::R_ARM_TLS_DTPOFF32:
  case llvm::ELF::R_ARM_TLS_TPOFF32:
    return true;
    break;

  default:
    return false;
  }
  return false;
}

void ARMRelocator::scanLocalReloc(InputFile &pInput, Relocation::Type Type,
                                  Relocation &pReloc,
                                  const ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  switch (Type) {

  // Set R_ARM_TARGET1 to R_ARM_ABS32
  // Ref: GNU gold 1.11 arm.cc, line 9892
  // FIXME: R_ARM_TARGET1 should be set by option --target1-rel
  // or --target1-rel
  case llvm::ELF::R_ARM_TARGET1:
    pReloc.setType(llvm::ELF::R_ARM_ABS32);
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_ARM_ABS32:
  case llvm::ELF::R_ARM_ABS32_NOI: {
    // If building PIC object (shared library or PIC executable),
    // a dynamic relocations with RELATIVE type to this location is needed.
    // Reserve an entry in .rel.dyn
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(),
                         llvm::ELF::R_ARM_RELATIVE, m_Target);
      rsym->setReserved(rsym->reserved() | ReserveRel);
      getTarget().checkAndSetHasTextRel(pSection);
    }
    return;
  }

  case llvm::ELF::R_ARM_ABS16:
  case llvm::ELF::R_ARM_ABS12:
  case llvm::ELF::R_ARM_THM_ABS5:
  case llvm::ELF::R_ARM_ABS8:
  case llvm::ELF::R_ARM_BASE_ABS:
  case llvm::ELF::R_ARM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_MOVT_ABS:
  case llvm::ELF::R_ARM_THM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_THM_MOVT_ABS: {
    // PIC code should not contain these kinds of relocation
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      config().raise(Diag::non_pic_relocation)
          << (int)pReloc.type() << pReloc.symInfo()->name()
          << pReloc.getSourcePath(config().options());
    }
    return;
  }
  case llvm::ELF::R_ARM_GOTOFF32:
  case llvm::ELF::R_ARM_GOTOFF12: {
    // FIXME: A GOT section is needed
    return;
  }

  case llvm::ELF::R_ARM_GOT_BREL:
  case llvm::ELF::R_ARM_GOT_PREL: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // A GOT entry is needed for these relocation type.
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    CreateGOT(Obj, pReloc, false, m_Target,
              (config().codeGenType() == LinkerConfig::Exec));
    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_ARM_BASE_PREL: {
    // FIXME: Currently we only support R_ARM_BASE_PREL against
    // symbol _GLOBAL_OFFSET_TABLE_
    if (rsym != getTarget().getGOTSymbol()->resolveInfo())
      config().raise(Diag::base_relocation)
          << (int)pReloc.type() << rsym->name();
    return;
  }
  case llvm::ELF::R_ARM_COPY:
  case llvm::ELF::R_ARM_GLOB_DAT:
  case llvm::ELF::R_ARM_JUMP_SLOT:
  case llvm::ELF::R_ARM_RELATIVE: {
    // These are relocation type for dynamic linker, should not
    // appear in object file.
    config().raise(Diag::dynamic_relocation) << (int)pReloc.type();
    m_Target.getModule().setFailure(true);
    return;
  }

  case llvm::ELF::R_ARM_TLS_GD32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;

    // set up a pair of got entries and a pair of dyn rel
    ARMGOT *G = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);
    if (config().isCodeStatic()) {
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->getFirst()->setReservedValue(1);
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      G->getNext()->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    // setup dyn rel for got entries against rsym
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_ARM_TLS_DTPMOD32, m_Target);
    helper_DynRel_init(Obj, &pReloc, rsym, G->getNext(), 0x0,
                       llvm::ELF::R_ARM_TLS_DTPOFF32, m_Target);

    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_ARM_TLS_LDM32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    ARMGOT *G = getTLSModuleID(pReloc.symInfo(), config().isCodeStatic());
    if (config().isCodeStatic()) {
      G->getFirst()->setReservedValue(1);
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      return;
    }
    return;
  }

  case llvm::ELF::R_ARM_TLS_IE32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;

    // set up the got and the corresponding rel entry
    ARMGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    if (config().isCodeStatic()) {
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0, llvm::ELF::R_ARM_TLS_TPOFF32,
                       m_Target);
    if (rsym->reserved() == Relocator::None)
      rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  default: {
    break;
  }
  } // end switch
}

void ARMRelocator::scanGlobalReloc(InputFile &pInput, Relocation::Type Type,
                                   Relocation &pReloc, eld::IRBuilder &pBuilder,
                                   ELFSection &pSection,
                                   CopyRelocs &CopyRelocs) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  switch (Type) {
  // Set R_ARM_TARGET1 to R_ARM_ABS32
  // Ref: GNU gold 1.11 arm.cc, line 9892
  // FIXME: R_ARM_TARGET1 should be set by option --target1-rel
  // or --target1-rel
  case llvm::ELF::R_ARM_TARGET1:
    pReloc.setType(llvm::ELF::R_ARM_ABS32);
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_ARM_ABS32:
  case llvm::ELF::R_ARM_ABS16:
  case llvm::ELF::R_ARM_ABS12:
  case llvm::ELF::R_ARM_THM_ABS5:
  case llvm::ELF::R_ARM_ABS8:
  case llvm::ELF::R_ARM_BASE_ABS:
  case llvm::ELF::R_ARM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_MOVT_ABS:
  case llvm::ELF::R_ARM_THM_MOVW_ABS_NC:
  case llvm::ELF::R_ARM_THM_MOVT_ABS:
  case llvm::ELF::R_ARM_ABS32_NOI: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);
    // Absolute relocation type, symbol may needs PLT entry or
    // dynamic relocation entry
    if (isSymbolPreemptible && (rsym->type() == ResolveInfo::Function)) {
      // create plt for this symbol if it does not have one
      if (!(rsym->reserved() & ReservePLT)) {
        // Symbol needs PLT entry, we need to reserve a PLT entry
        // and the corresponding GOT and dynamic relocation entry
        // in .got and .rel.plt.
        m_Target.createPLT(Obj, rsym);
        // set PLT bit
        rsym->setReserved(rsym->reserved() | ReservePLT);
      }
    }

    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      true)) {
      if (getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
        // check if the option -z nocopyreloc is given
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << pInput.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }
        CopyRelocs.insert(rsym);
      } else {
        if (!checkValidReloc(pReloc)) {
          config().raise(Diag::non_pic_relocation)
              << (int)pReloc.type() << pReloc.symInfo()->name()
              << pReloc.getSourcePath(config().options());
          m_Target.getModule().setFailure(true);
          return;
        }
        helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                           pReloc.targetRef()->offset(),
                           isSymbolPreemptible ? llvm::ELF::R_ARM_ABS32
                                               : llvm::ELF::R_ARM_RELATIVE,
                           m_Target);
        rsym->setReserved(rsym->reserved() | ReserveRel);
        getTarget().checkAndSetHasTextRel(pSection);
      }
    }
    return;
  }

  case llvm::ELF::R_ARM_GOTOFF32:
  case llvm::ELF::R_ARM_GOTOFF12: {
    // FIXME: A GOT section is needed
    return;
  }

  case llvm::ELF::R_ARM_BASE_PREL:
  case llvm::ELF::R_ARM_THM_MOVW_BREL_NC:
  case llvm::ELF::R_ARM_THM_MOVW_BREL:
  case llvm::ELF::R_ARM_THM_MOVT_BREL:
    // FIXME: Currently we only support these relocations against
    // symbol _GLOBAL_OFFSET_TABLE_
    if (rsym != getTarget().getGOTSymbol()->resolveInfo()) {
      config().raise(Diag::base_relocation)
          << (int)pReloc.type() << rsym->name() << "ARM/Hexagon Support";
      m_Target.getModule().setFailure(true);
      return;
    }
    LLVM_FALLTHROUGH;
  case llvm::ELF::R_ARM_REL32:
  case llvm::ELF::R_ARM_LDR_PC_G0:
  case llvm::ELF::R_ARM_SBREL32:
  case llvm::ELF::R_ARM_THM_PC8:
  case llvm::ELF::R_ARM_MOVW_PREL_NC:
  case llvm::ELF::R_ARM_MOVT_PREL:
  case llvm::ELF::R_ARM_THM_MOVW_PREL_NC:
  case llvm::ELF::R_ARM_THM_MOVT_PREL:
  case llvm::ELF::R_ARM_THM_ALU_PREL_11_0:
  case llvm::ELF::R_ARM_THM_PC12:
  case llvm::ELF::R_ARM_REL32_NOI:
  case llvm::ELF::R_ARM_ALU_PC_G0_NC:
  case llvm::ELF::R_ARM_ALU_PC_G0:
  case llvm::ELF::R_ARM_ALU_PC_G1_NC:
  case llvm::ELF::R_ARM_ALU_PC_G1:
  case llvm::ELF::R_ARM_ALU_PC_G2:
  case llvm::ELF::R_ARM_LDR_PC_G1:
  case llvm::ELF::R_ARM_LDR_PC_G2:
  case llvm::ELF::R_ARM_LDRS_PC_G0:
  case llvm::ELF::R_ARM_LDRS_PC_G1:
  case llvm::ELF::R_ARM_LDRS_PC_G2:
  case llvm::ELF::R_ARM_LDC_PC_G0:
  case llvm::ELF::R_ARM_LDC_PC_G1:
  case llvm::ELF::R_ARM_LDC_PC_G2:
  case llvm::ELF::R_ARM_ALU_SB_G0_NC:
  case llvm::ELF::R_ARM_ALU_SB_G0:
  case llvm::ELF::R_ARM_ALU_SB_G1_NC:
  case llvm::ELF::R_ARM_ALU_SB_G1:
  case llvm::ELF::R_ARM_ALU_SB_G2:
  case llvm::ELF::R_ARM_LDR_SB_G0:
  case llvm::ELF::R_ARM_LDR_SB_G1:
  case llvm::ELF::R_ARM_LDR_SB_G2:
  case llvm::ELF::R_ARM_LDRS_SB_G0:
  case llvm::ELF::R_ARM_LDRS_SB_G1:
  case llvm::ELF::R_ARM_LDRS_SB_G2:
  case llvm::ELF::R_ARM_LDC_SB_G0:
  case llvm::ELF::R_ARM_LDC_SB_G1:
  case llvm::ELF::R_ARM_LDC_SB_G2:
  case llvm::ELF::R_ARM_MOVW_BREL_NC:
  case llvm::ELF::R_ARM_MOVT_BREL:
  case llvm::ELF::R_ARM_MOVW_BREL: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Relative addressing relocation, may needs dynamic relocation

    if (getTarget().symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                      false)) {
      // symbol needs dynamic relocation entry, reserve an entry in .rel.dyn
      if (getTarget().symbolNeedsCopyReloc(pReloc, *rsym)) {
        CopyRelocs.insert(rsym);
      } else {
        if (!checkValidReloc(pReloc)) {
          config().raise(Diag::non_pic_relocation)
              << (int)pReloc.type() << pReloc.symInfo()->name()
              << pReloc.getSourcePath(config().options());
          m_Target.getModule().setFailure(true);
          return;
        }
        rsym->setReserved(rsym->reserved() | ReserveRel);
        getTarget().checkAndSetHasTextRel(pSection);
      }
    }
    return;
  }

  case llvm::ELF::R_ARM_PC24:
  case llvm::ELF::R_ARM_THM_CALL:
  case llvm::ELF::R_ARM_PLT32:
  case llvm::ELF::R_ARM_CALL:
  case llvm::ELF::R_ARM_JUMP24:
  case llvm::ELF::R_ARM_THM_JUMP24:
  case llvm::ELF::R_ARM_SBREL31:
  case llvm::ELF::R_ARM_PREL31:
  case llvm::ELF::R_ARM_THM_JUMP19:
  case llvm::ELF::R_ARM_THM_JUMP6:
  case llvm::ELF::R_ARM_THM_JUMP11:
  case llvm::ELF::R_ARM_THM_JUMP8: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // These are branch relocation (except PREL31)
    // A PLT entry is needed when building shared library

    // return if we already create plt for this symbol
    if (rsym->reserved() & ReservePLT)
      return;

    // create IRELATIVE for IFUNC symbol
    if ((rsym->type() == ResolveInfo::IndirectFunc) &&
        (config().isCodeStatic())) {
      m_Target.createPLT(Obj, rsym, true);
      rsym->setReserved(rsym->reserved() | ReservePLT);
      ARMGNULDBackend &backend = getTarget();
      backend.defineIRelativeRange(*rsym);
      return;
    }

    // if symbol is defined in the output file and it's not
    // preemptible, no need plt
    if (!getTarget().isSymbolPreemptible(*rsym))
      return;
    // Hidden symbol will not go into dynsym
    if (rsym->visibility() == ResolveInfo::Hidden)
      return;

    // Symbol needs PLT entry, we need to reserve a PLT entry
    // and the corresponding GOT and dynamic relocation entry
    // in .got and .rel.plt.
    m_Target.createPLT(Obj, rsym);
    // set PLT bit
    rsym->setReserved(rsym->reserved() | ReservePLT);
    return;
  }

  case llvm::ELF::R_ARM_GOT_BREL:
  case llvm::ELF::R_ARM_GOT_ABS:
  case llvm::ELF::R_ARM_GOT_PREL: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    // if the symbol cannot be fully resolved at link time, then we need a
    // dynamic relocation
    CreateGOT(Obj, pReloc, !config().isCodeStatic(), m_Target,
              config().codeGenType() == LinkerConfig::Exec);
    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_ARM_COPY:
  case llvm::ELF::R_ARM_GLOB_DAT:
  case llvm::ELF::R_ARM_JUMP_SLOT:
  case llvm::ELF::R_ARM_RELATIVE: {
    // These are relocation type for dynamic linker, should not
    // appear in object file.
    config().raise(Diag::dynamic_relocation) << (int)pReloc.type();
    m_Target.getModule().setFailure(true);
    return;
  }

  case llvm::ELF::R_ARM_TLS_GD32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;

    // set up a pair of got entries and a pair of dyn rel
    ARMGOT *G = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);

    if (config().isCodeStatic()) {
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->getFirst()->setReservedValue(1);
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      G->getNext()->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    // setup dyn rel for got entries against rsym
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       llvm::ELF::R_ARM_TLS_DTPMOD32, m_Target);
    helper_DynRel_init(Obj, &pReloc, rsym, G->getNext(), 0x0,
                       llvm::ELF::R_ARM_TLS_DTPOFF32, m_Target);

    // set GOT bit
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  } break;

  case llvm::ELF::R_ARM_TLS_LDM32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    ARMGOT *G = getTLSModuleID(pReloc.symInfo(), config().isCodeStatic());
    if (config().isCodeStatic()) {
      G->getFirst()->setReservedValue(1);
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      return;
    }
    return;
  }

  case llvm::ELF::R_ARM_TLS_IE32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;

    // set up the got and the corresponding rel entry
    ARMGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    if (config().isCodeStatic()) {
      rsym->setReserved(rsym->reserved() | ReserveGOT);
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0, llvm::ELF::R_ARM_TLS_TPOFF32,
                       m_Target);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  default: {
    break;
  }
  } // end switch
}

void ARMRelocator::scanRelocation(Relocation &pReloc, eld::IRBuilder &pBuilder,
                                  ELFSection &pSection, InputFile &pInputFile,
                                  CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  if (isInvalidReloc(pReloc)) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    config().raise(Diag::non_pic_relocation)
        << getName(pReloc.type()) << pReloc.symInfo()->name()
        << pReloc.getSourcePath(config().options());
    m_Target.getModule().setFailure(true);
    return;
  }

  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  assert(nullptr != rsym &&
         "ResolveInfo of relocation not set while scanRelocation");

  // Check if we are tracing relocations.
  if (m_Module.getPrinter()->traceReloc()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    std::string relocName = getName(pReloc.type());
    if (config().options().traceReloc(relocName))
      config().raise(Diag::reloc_trace)
          << relocName << pReloc.symInfo()->name()
          << pInputFile.getInput()->decoratedPath();
  }

  // check if we should issue undefined reference for the relocation target
  // symbol
  {
    if (rsym->isUndef() || rsym->isBitCode()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      if (m_Target.canIssueUndef(rsym)) {
        if (rsym->visibility() != ResolveInfo::Default)
          issueInvisibleRef(pReloc, pInputFile);
        issueUndefRef(pReloc, pInputFile, &pSection);
      }
    }
  }

  ELFSection *section = pSection.getLink()
                            ? pSection.getLink()
                            : pReloc.targetRef()->frag()->getOwningSection();

  if (!section->isAlloc())
    return;

  Relocation::Type Type = pReloc.type();

  // Set R_ARM_TARGET2 to R_ARM_ABS32/R_ARM_REL32/R_ARM_GOT_PREL.
  if (Type == llvm::ELF::R_ARM_TARGET2) {
    switch (config().options().getTarget2Policy()) {
    case GeneralOptions::Target2Policy::Abs:
      Type = llvm::ELF::R_ARM_ABS32;
      break;
    case GeneralOptions::Target2Policy::Rel:
      Type = llvm::ELF::R_ARM_REL32;
      break;
    case GeneralOptions::Target2Policy::GotRel:
      Type = llvm::ELF::R_ARM_GOT_PREL;
      break;
    }
  }

  if (rsym->isLocal()) // rsym is local
    scanLocalReloc(pInputFile, Type, pReloc, *section);
  else // rsym is external
    scanGlobalReloc(pInputFile, Type, pReloc, pBuilder, *section, CopyRelocs);
}

//=========================================//
// Each relocation function implementation //
//=========================================//

// R_ARM_NONE
Relocator::Result none(Relocation &pReloc, ARMRelocator &pParent) {
  return Relocator::OK;
}

// R_ARM_ABS32: (S + A) | T
Relocator::Result abs32(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord A = pReloc.target() + pReloc.addend();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // If the flag of target section is not ALLOC, we will not scan this
  // relocation
  // but perform static relocation. (e.g., applying .debug section)
  if (!pReloc.targetRef()->getOutputELFSection()->isAlloc()) {
    pReloc.target() = (S + A) | T;
    return Relocator::OK;
  }

  if (rsym && (rsym->reserved() & Relocator::ReserveRel) &&
      (pParent.getTarget().isSymbolPreemptible(*rsym)))
    return Relocator::OK;

  if (rsym && rsym->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);
    T = 0;
  }

  if (rsym && rsym->isWeakUndef() &&
      (pParent.config().codeGenType() == LinkerConfig::Exec))
    S = 0;

  // perform static relocation
  pReloc.target() = (S + A) | T;
  return Relocator::OK;
}

// R_ARM_REL32: ((S + A) | T) - P
// R_ARM_SBREL32: ((S + A) | T) - B(S)
Relocator::Result rel32(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // perform static relocation
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord A = pReloc.target() + pReloc.addend();
  Relocator::Address P = pReloc.place(pParent.module());

  // An external symbol may need PLT (this reloc is from a stub/veneer)
  if (!pReloc.symInfo()->isLocal()) {
    if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget()
              .findEntryInPLT(pReloc.symInfo())
              ->getAddr(DiagEngine);
      T = 0; // PLT is not thumb.
    }
  }

  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // perform relocation
  if (pReloc.type() == llvm::ELF::R_ARM_SBREL32) {
    std::lock_guard<std::mutex> relocGuard(m_ApplyRelocMutex);
    ELFSegment *S = pParent.getSBRELSegment();
    ELFSegment *RSeg = nullptr;
    // If the symbol is not associated with any fragment, the symbol
    // may be a linker script symbol. Figure out where the linker
    // script symbol belongs to.
    if (!pReloc.symInfo()->outSymbol()->fragRef()->frag()) {
      ELFSection *SInfo =
          pParent.getTarget().getSectionInfo(pReloc.symInfo()->outSymbol());
      if (!SInfo || !SInfo->getOutputSection()) {
        DiagEngine->raise(Diag::sbrel_reloc_no_section)
            << pReloc.symInfo()->name();
        return Relocator::BadReloc;
      }
      RSeg = SInfo->getOutputSection()->getLoadSegment();
    } else {
      RSeg = pReloc.getTargetSegment();
    }
    if (!S) {
      S = RSeg;
      pParent.setSBRELSegment(RSeg);
    }
    if (RSeg && (RSeg != S)) {
      DiagEngine->raise(Diag::sbrel_reloc_different_seg)
          << pReloc.symInfo()->name()
          << pReloc.getSourcePath(pParent.config().options());
      return Relocator::BadReloc;
    }
    P = RSeg->vaddr();
  }

  pReloc.target() = ((S + A) | T) - P;
  return Relocator::OK;
}

// R_ARM_BASE_PREL: B(S) + A - P
Relocator::Result base_prel(Relocation &pReloc, ARMRelocator &pParent) {
  // perform static relocation
  Relocator::DWord A = pReloc.target() + pReloc.addend();
  pReloc.target() =
      pParent.getSymValue(&pReloc) + A - pReloc.place(pParent.module());
  return Relocator::OK;
}

// R_ARM_GOTOFF32: ((S + A) | T) - GOT_ORG
Relocator::Result gotoff32(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord A = pReloc.target() + pReloc.addend();
  Relocator::Address GOT_ORG = pParent.getTarget().getGOTSymbolAddr();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  pReloc.target() = ((S + A) | T) - GOT_ORG;
  return Relocator::OK;
}

// R_ARM_GOT_BREL: GOT(S) + A - GOT_ORG
Relocator::Result got_brel(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT))
    return Relocator::BadReloc;

  Relocator::Address GOT_S =
      pParent.getTarget().findEntryInGOT(pReloc.symInfo())->getAddr(DiagEngine);
  Relocator::DWord A = pReloc.target() + pReloc.addend();
  Relocator::Address GOT_ORG = pParent.getTarget().getGOTSymbolAddr();
  // Apply relocation.
  pReloc.target() = GOT_S + A - GOT_ORG;

  return Relocator::OK;
}

// R_ARM_GOT_PREL: GOT(S) + A - P
Relocator::Result got_prel(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }
  Relocator::Address GOT_S =
      pParent.getTarget().findEntryInGOT(pReloc.symInfo())->getAddr(DiagEngine);
  Relocator::DWord A = pReloc.addend() + pReloc.target();
  Relocator::Address P = pReloc.place(pParent.module());

  // Apply relocation.
  pReloc.target() = GOT_S + A - P;

  return Relocator::OK;
}

// R_ARM_THM_JUMP8: S + A - P
Relocator::Result thm_jump8(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord A =
      llvm::SignExtend64((pReloc.target() & 0x00ff) << 1, 8) + pReloc.addend();
  // S depends on PLT exists or not
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);

  Relocator::DWord X = S + A - P;
  if (helper_check_signed_overflow(X, 9))
    return Relocator::Overflow;
  //                    Make sure the Imm is 0.          Result Mask.
  pReloc.target() = (pReloc.target() & 0xFFFFFF00u) | ((X & 0x01FEu) >> 1);
  return Relocator::OK;
}

// R_ARM_THM_JUMP11: S + A - P
Relocator::Result thm_jump11(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord A =
      llvm::SignExtend64((pReloc.target() & 0x07ff) << 1, 11) + pReloc.addend();
  // S depends on PLT exists or not
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT)
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);

  Relocator::DWord X = S + A - P;
  if (helper_check_signed_overflow(X, 12))
    return Relocator::Overflow;
  //                    Make sure the Imm is 0.          Result Mask.
  pReloc.target() = (pReloc.target() & 0xFFFFF800u) | ((X & 0x0FFEu) >> 1);
  return Relocator::OK;
}

// R_ARM_THM_JUMP19: ((S + A) | T) - P
Relocator::Result thm_jump19(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);

  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ true);
  Relocator::DWord A =
      helper_thumb32_cond_branch_offset(upper_inst, lower_inst);
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::Address S;
  // if symbol has plt
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);
    T = 0; // PLT is not thumb.
  } else {
    S = pParent.getSymValue(&pReloc);
    if (T != 0x0)
      helper_clear_thumb_bit(S);
  }

  if (0x0 == T) {
    // FIXME: conditional branch to PLT in THUMB-2 not supported yet
    DiagEngine->raise(Diag::unsupport_cond_branch_reloc) << (int)pReloc.type();
    return Relocator::BadReloc;
  }

  Relocator::DWord X = ((S + A) | T) - P;
  if (helper_check_signed_overflow(X, 21))
    return Relocator::Overflow;

  upper_inst = helper_thumb32_cond_branch_upper(upper_inst, X);
  lower_inst = helper_thumb32_cond_branch_lower(lower_inst, X);

  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = upper_inst;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = lower_inst;

  return Relocator::OK;
}

// R_ARM_ALU_PC_G0: ((S + A) | T) - P
Relocator::Result alu_pc(Relocation &pReloc, ARMRelocator &pParent) {
  // perform static relocation
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::Address P = pReloc.place(pParent.module());

  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // 5.6.1.1 If the instruction is a SUB, or an LDR/STR type instruction with
  // the ‘up’ bit clear, then the initial addend is formed by negating the
  // unsigned immediate value encoded in the instruction.
  uint32_t I = pReloc.target();
  uint32_t A = llvm::rotr<uint32_t>(I & 0xff, ((I & 0xf00) >> 8) * 2);
  if (I & 0x00400000)
    A = -A;

  I &= 0xff3ff000;

  // Choose instruction type (ADD or SUB) and compute the absolute value.
  uint32_t X = ((S + A) | T) - P;
  if ((int32_t)X >= 0)
    I |= 0x00800000; // ADD
  else {
    I |= 0x00400000; // SUB
    X = -X;
  }

  if (X) {
    // Find the current value bit length and the value K_n.
    unsigned L = llvm::bit_width<uint32_t>(X);
    unsigned K = (L < 8 ? 0 : L - 7) >> 1;
    // Encode the shifted immediate.
    I |= ((X >> (K * 2)) & 0xff) | ((16 - K) & 0xf) << 8;
    // Mask off used bits, the residual will be needed for other groups.
    X &= llvm::maskTrailingOnes<uint32_t>(K * 2);
    // For higher groups, the above is repeated.
    // If there is still residual, the value cannot be represented.
    if (X)
      return Relocator::Overflow;
  }

  pReloc.target() = I;

  return Relocator::OK;
}

// R_ARM_PC24: ((S + A) | T) - P
// R_ARM_PLT32: ((S + A) | T) - P
// R_ARM_JUMP24: ((S + A) | T) - P
// R_ARM_CALL: ((S + A) | T) - P
Relocator::Result call(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // If target is undefined weak symbol, we only need to jump to the
  // next instruction unless it has PLT entry. Rewrite instruction
  // to NOP.
  if (pReloc.symInfo()->isWeak() && pReloc.symInfo()->isUndef() &&
      !pReloc.symInfo()->isDyn() &&
      !(pReloc.symInfo()->reserved() & Relocator::ReservePLT)) {
    // change target to NOP : mov r0, r0
    pReloc.target() = (pReloc.target() & 0xf0000000U) | 0x01a00000;
    return Relocator::OK;
  }

  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ true);
  Relocator::DWord A =
      llvm::SignExtend64((pReloc.target() & 0x00FFFFFFu) << 2, 26) +
      pReloc.addend();
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // S depends on PLT exists or not
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);
    T = 0; // PLT is not thumb.
  }

  // At this moment (after relaxation), if the jump target is thumb instruction,
  // switch mode is needed, rewrite the instruction to BLX
  if (T != 0 && pParent.getTarget().canRewriteToBLX()) {
    // cannot rewrite to blx for R_ARM_JUMP24
    if (pReloc.type() == llvm::ELF::R_ARM_JUMP24)
      return Relocator::BadReloc;
    if (pReloc.type() == llvm::ELF::R_ARM_PC24)
      return Relocator::BadReloc;

    pReloc.target() =
        (pReloc.target() & 0xffffff) | 0xfa000000 | (((S + A - P) & 2) << 23);
  }

  Relocator::DWord X = ((S + A) | T) - P;
  // Check X is 24bit sign int. If not, we should use stub or PLT before apply.
  if (helper_check_signed_overflow(X, 26))
    return Relocator::Overflow;
  //                    Make sure the Imm is 0.          Result Mask.
  pReloc.target() = (pReloc.target() & 0xFF000000u) | ((X & 0x03FFFFFEu) >> 2);

  // if Target is ARM, we need to rewrite BLX to BL.
  if (T == 0 && (pReloc.target() >> 25) == 0x7D)
    pReloc.target() = (pReloc.target() & 0x00FFFFFFu) | (0xEB << 24);

  return Relocator::OK;
}

// R_ARM_THM_CALL: ((S + A) | T) - P
// R_ARM_THM_JUMP24: ((S + A) | T) - P
Relocator::Result thm_call(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  // If target is undefined weak symbol, we only need to jump to the
  // next instruction unless it has PLT entry. Rewrite instruction
  // to NOP.
  if (pReloc.symInfo()->isWeak() && pReloc.symInfo()->isUndef() &&
      !pReloc.symInfo()->isDyn() &&
      !(pReloc.symInfo()->reserved() & Relocator::ReservePLT)) {
    pReloc.target() = (0xbf00U << 16) | 0xe000U;
    return Relocator::OK;
  }

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);

  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ true);
  Relocator::DWord A = helper_thumb32_branch_offset(upper_inst, lower_inst);
  Relocator::Address P = pReloc.place(pParent.module());
  Relocator::Address S;

  // if symbol has plt
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);
    T = 0; // PLT is not thumb.
  } else {
    S = pParent.getSymValue(&pReloc);
    if (T != 0x0)
      helper_clear_thumb_bit(S);
  }

  S = S + A;

  // At this moment (after relaxation), if the jump target is arm
  // instruction, switch mode is needed, rewrite the instruction to BLX
  if (T == 0 && pParent.getTarget().canRewriteToBLX()) {
    if (pReloc.type() != llvm::ELF::R_ARM_THM_JUMP24) {
      // for BLX, select bit 1 from relocation base address to jump target
      // address
      S = helper_bit_select(S, P, 0x2);
      // rewrite instruction to BLX
      lower_inst &= ~0x1000U;
    }
  } else {
    // otherwise, the instruction should be BL
    lower_inst |= 0x1000U;
  }

  Relocator::DWord X = (S | T) - P;

  // FIXME: Check bit size is 24(thumb2) or 22?
  if (helper_check_signed_overflow(X, 25)) {
    return Relocator::Overflow;
  }

  upper_inst = helper_thumb32_branch_upper(upper_inst, X);
  lower_inst = helper_thumb32_branch_lower(lower_inst, X);

  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = upper_inst;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = lower_inst;

  return Relocator::OK;
}

// R_ARM_MOVW_ABS_NC: (S + A) | T
Relocator::Result movw_abs_nc(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord A =
      helper_extract_movw_movt_addend(pReloc.target()) + pReloc.addend();
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();

  // If the flag of target section is not ALLOC, we will not scan this
  // relocation but perform static relocation. (e.g., applying .debug section)
  if (target_sect->isAlloc()) {
    // use plt
    if (rsym->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(DiagEngine);
      T = 0; // PLT is not thumb
    }
  }

  // perform static relocation
  Relocator::DWord X = (S + A) | T;
  pReloc.target() =
      helper_insert_val_movw_movt_inst(pReloc.target() + pReloc.addend(), X);
  return Relocator::OK;
}

// R_ARM_MOVW_PREL_NC: ((S + A) | T) - P
Relocator::Result movw_prel_nc(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord A =
      helper_extract_movw_movt_addend(pReloc.target()) + pReloc.addend();
  if (T != 0x0)
    helper_clear_thumb_bit(S);
  Relocator::DWord X = ((S + A) | T) - P;
  pReloc.target() = helper_insert_val_movw_movt_inst(pReloc.target(), X);
  return Relocator::OK;
}

// R_ARM_MOVT_ABS: S + A
Relocator::Result movt_abs(Relocation &pReloc, ARMRelocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A =
      helper_extract_movw_movt_addend(pReloc.target()) + pReloc.addend();

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();

  // If the flag of target section is not ALLOC, we will not scan this
  // relocation
  // but perform static relocation. (e.g., applying .debug section)
  if (target_sect->isAlloc()) {
    // use plt
    if (rsym->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
          pParent.config().getDiagEngine());
    }
  }

  Relocator::DWord X = S + A;
  X >>= 16;
  // perform static relocation
  pReloc.target() = helper_insert_val_movw_movt_inst(pReloc.target(), X);
  return Relocator::OK;
}

// R_ARM_MOVT_PREL: S + A - P
Relocator::Result movt_prel(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::DWord A =
      helper_extract_movw_movt_addend(pReloc.target()) + pReloc.addend();
  Relocator::DWord X = S + A - P;
  X >>= 16;

  pReloc.target() = helper_insert_val_movw_movt_inst(pReloc.target(), X);
  return Relocator::OK;
}

// R_ARM_THM_MOVW_ABS_NC: (S + A) | T
Relocator::Result thm_movw_abs_nc(Relocation &pReloc, ARMRelocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);
  Relocator::DWord val = ((upper_inst) << 16) | (lower_inst);
  Relocator::DWord A =
      helper_extract_thumb_movw_movt_addend(val) + pReloc.addend();

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();
  // If the flag of target section is not ALLOC, we will not scan this
  // relocation
  // but perform static relocation. (e.g., applying .debug section)
  if (target_sect->isAlloc()) {
    // use plt
    if (rsym->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
          pParent.config().getDiagEngine());
      T = 0; // PLT is not thumb
    }
  }
  Relocator::DWord X = (S + A) | T;

  val = helper_insert_val_thumb_movw_movt_inst(val, X);
  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = val >> 16;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = val & 0xFFFFu;

  return Relocator::OK;
}

// R_ARM_THM_MOVW_PREL_NC: ((S + A) | T) - P
Relocator::Result thm_movw_prel_nc(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord P = pReloc.place(pParent.module());
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);
  Relocator::DWord val = ((upper_inst) << 16) | (lower_inst);
  Relocator::DWord A =
      helper_extract_thumb_movw_movt_addend(val) + pReloc.addend();
  Relocator::DWord X = ((S + A) | T) - P;

  val = helper_insert_val_thumb_movw_movt_inst(val, X);
  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = val >> 16;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = val & 0xFFFFu;

  return Relocator::OK;
}

// R_ARM_THM_MOVW_BREL_NC: ((S + A) | T) - B(S)
// R_ARM_THM_MOVW_BREL: ((S + A) | T) - B(S)
Relocator::Result thm_movw_brel(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord P = pReloc.place(pParent.module());
  ;
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);
  Relocator::DWord val = ((upper_inst) << 16) | (lower_inst);
  Relocator::DWord A =
      helper_extract_thumb_movw_movt_addend(val) + pReloc.addend();

  Relocator::DWord X = ((S + A) | T) - P;

  val = helper_insert_val_thumb_movw_movt_inst(val, X);
  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = val >> 16;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = val & 0xFFFFu;

  return Relocator::OK;
}

// R_ARM_THM_MOVT_ABS: S + A
Relocator::Result thm_movt_abs(Relocation &pReloc, ARMRelocator &pParent) {
  ResolveInfo *rsym = pReloc.symInfo();
  Relocator::Address S = pParent.getSymValue(&pReloc);

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);
  Relocator::DWord val = ((upper_inst) << 16) | (lower_inst);
  Relocator::DWord A =
      helper_extract_thumb_movw_movt_addend(val) + pReloc.addend();

  ELFSection *target_sect = pReloc.targetRef()->getOutputELFSection();
  // If the flag of target section is not ALLOC, we will not scan this
  // relocation but perform static relocation. (e.g., applying .debug section)
  if (target_sect->isAlloc()) {
    // use plt
    if (rsym->reserved() & Relocator::ReservePLT) {
      S = pParent.getTarget().findEntryInPLT(rsym)->getAddr(
          pParent.config().getDiagEngine());
    }
  }

  Relocator::DWord X = S + A;
  X >>= 16;

  val = helper_insert_val_thumb_movw_movt_inst(val, X);
  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = val >> 16;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = val & 0xFFFFu;
  return Relocator::OK;
}

// R_ARM_THM_MOVT_PREL: S + A - P
// R_ARM_THM_MOVT_BREL: S + A - B(S)
Relocator::Result thm_movt_prel(Relocation &pReloc, ARMRelocator &pParent) {
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord P = pReloc.place(pParent.module());

  // get lower and upper 16 bit instructions from relocation targetData
  uint16_t upper_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()));
  uint16_t lower_inst = *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1);
  Relocator::DWord val = ((upper_inst) << 16) | (lower_inst);
  Relocator::DWord A =
      helper_extract_thumb_movw_movt_addend(val) + pReloc.addend();
  Relocator::DWord X = S + A - P;
  X >>= 16;

  val = helper_insert_val_thumb_movw_movt_inst(val, X);
  *(reinterpret_cast<uint16_t *>(&pReloc.target())) = val >> 16;
  *(reinterpret_cast<uint16_t *>(&pReloc.target()) + 1) = val & 0xFFFFu;

  return Relocator::OK;
}

Relocator::Result target2(Relocation &R, ARMRelocator &Parent) {
  switch (Parent.config().options().getTarget2Policy()) {
  case GeneralOptions::Target2Policy::Abs:
    return abs32(R, Parent);
  case GeneralOptions::Target2Policy::Rel:
    return rel32(R, Parent);
  case GeneralOptions::Target2Policy::GotRel:
    return got_prel(R, Parent);
  }
}

// R_ARM_PREL31: ((S + A) | T) - P
Relocator::Result prel31(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  Relocator::DWord target = pReloc.target();
  Relocator::DWord T = getThumbBit(pParent, pReloc, /*IsJump*/ false);
  Relocator::DWord A = llvm::SignExtend64(target, 31) + pReloc.addend();
  Relocator::DWord P = pReloc.place(pParent.module());
  Relocator::Address S = pParent.getSymValue(&pReloc);
  if (T != 0x0)
    helper_clear_thumb_bit(S);

  // if symbol has plt
  if (pReloc.symInfo()->reserved() & Relocator::ReservePLT) {
    S = pParent.getTarget()
            .findEntryInPLT(pReloc.symInfo())
            ->getAddr(DiagEngine);
    T = 0; // PLT is not thumb.
  }

  Relocator::DWord X = ((S + A) | T) - P;
  pReloc.target() = helper_bit_select(target, X, 0x7fffffffU);
  if (helper_check_signed_overflow(X, 31))
    return Relocator::Overflow;
  return Relocator::OK;
}

// R_ARM_TLS_GD32: GOT(S) + A - P
// R_ARM_TLS_LDM32: GOT(S) + A - P
// R_ARM_TLS_IE32: GOT(S) + A - P
Relocator::Result tls(Relocation &pReloc, ARMRelocator &pParent) {
  DiagnosticEngine *DiagEngine = pParent.config().getDiagEngine();
  ARMGOT *entry = pParent.getTarget().findEntryInGOT(pReloc.symInfo());
  ARMRelocator::Address GOT_S = entry->getAddr(DiagEngine);
  Relocator::DWord A = pReloc.addend();
  ARMRelocator::Address P = pReloc.place(pParent.module());

  // Apply relocation.
  pReloc.target() += GOT_S + A - P;
  return Relocator::OK;
}

// R_ARM_TLS_LDO32: S + A - TLS
Relocator::Result tls_ldo32(Relocation &pReloc, ARMRelocator &pParent) {
  pReloc.target() += pParent.getSymValue(&pReloc) + pReloc.addend();
  return Relocator::OK;
}

// R_ARM_TLS_LE32: S + A - tp
Relocator::Result tls_le32(Relocation &pReloc, ARMRelocator &pParent) {
  pReloc.target() += pParent.getSymValue(&pReloc) + pReloc.addend() + 0x8;
  return Relocator::OK;
}

Relocator::Result unsupport(Relocation &pReloc, ARMRelocator &pParent) {
  return Relocator::Unsupport;
}

// R_ARM_ADD_PREL_20_8 : (S + A - P) >> 20 & 0xFF
Relocator::Result relocAddPREL1(Relocation &pReloc, ARMRelocator &pParent) {
  // perform static relocation
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = (S + A) - pReloc.place(pParent.module());
  pReloc.target() = pReloc.target() | ((X >> 20) & 0xff);
  return Relocator::OK;
}

// R_ARM_ADD_PREL_12_8
Relocator::Result relocAddPREL2(Relocation &pReloc, ARMRelocator &pParent) {
  // perform static relocation
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = (S + A) - pReloc.place(pParent.module());
  pReloc.target() = pReloc.target() | ((X >> 12) & 0xff);
  return Relocator::OK;
}

// R_ARM_LDR_PREL_12
Relocator::Result relocLDR12(Relocation &pReloc, ARMRelocator &pParent) {
  // perform static relocation
  Relocator::Address S = pParent.getSymValue(&pReloc);
  Relocator::DWord A = pReloc.addend();
  Relocator::DWord X = (S + A) - pReloc.place(pParent.module());
  pReloc.target() = pReloc.target() | (X & 0xfff);
  return Relocator::OK;
}
