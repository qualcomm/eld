//===- RISCVRelocator.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "RISCVRelocator.h"
#include "RISCVLDBackend.h"
#include "RISCVLLVMExtern.h"
#include "RISCVPLT.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/Resolver.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"

namespace eld {

namespace {

struct RelocationDescription;

typedef Relocator::Result (*ApplyFunctionType)(
    eld::Relocation &pReloc, eld::RISCVLDBackend &,
    RelocationDescription &pRelocDesc);

struct RelocationDescription {
  // The application function for the relocation.
  const ApplyFunctionType func;
  // The Relocation type, this is just kept for convenience when writing new
  // handlers for relocations.
  const Relocator::Type type;
  // If the user specified, the relocation to be force verified, the relocation
  // is verified for alignment, truncation errors(only for relocations that take
  // in non signed values, signed values are bound to exceed the number of
  // bits).
  bool forceVerify;
};

#define DECL_RISCV_APPLY_RELOC_FUNC(Name)                                      \
  RISCVRelocator::Result Name(Relocation &pEntry, RISCVLDBackend &,            \
                              RelocationDescription &pRelocDesc);

DECL_RISCV_APPLY_RELOC_FUNC(unsupported)
DECL_RISCV_APPLY_RELOC_FUNC(applyNone)
DECL_RISCV_APPLY_RELOC_FUNC(applyAbs)
DECL_RISCV_APPLY_RELOC_FUNC(applyRel)
DECL_RISCV_APPLY_RELOC_FUNC(applyHI)
DECL_RISCV_APPLY_RELOC_FUNC(applyLO)
DECL_RISCV_APPLY_RELOC_FUNC(applyJumpOrCall)
DECL_RISCV_APPLY_RELOC_FUNC(applyAlign)
DECL_RISCV_APPLY_RELOC_FUNC(applyGPRel)
DECL_RISCV_APPLY_RELOC_FUNC(applyCompressedLUI)
DECL_RISCV_APPLY_RELOC_FUNC(applyTprelAdd)
DECL_RISCV_APPLY_RELOC_FUNC(applyGOT)
DECL_RISCV_APPLY_RELOC_FUNC(applyVendor)

#undef DECL_RISCV_APPLY_RELOC_FUNC

typedef std::unordered_map<Relocator::Type, RelocationDescription>
    RelocationDescMap;

#define PUBLIC_RELOC_DESC_ENTRY(type, fptr)                                    \
  {                                                                            \
    llvm::ELF::type, {                                                         \
      /*func=*/fptr, /*type=*/llvm::ELF::type, /*forceVerify=*/false           \
    }                                                                          \
  }

#define INTERNAL_RELOC_DESC_ENTRY(type, fptr)                                  \
  {                                                                            \
    eld::ELF::riscv::internal::type, {                                         \
      /*func=*/fptr, /*type=*/eld::ELF::riscv::internal::type,                 \
          /*forceVerify=*/false                                                \
    }                                                                          \
  }

/* Not const: the `forceVerify` entries might be changed. */
RelocationDescMap RelocDescs = {
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_NONE, applyNone),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_32, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_64, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_RELATIVE, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_COPY, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_JUMP_SLOT, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_DTPMOD32, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_DTPMOD64, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_DTPREL32, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_DTPREL64, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_TPREL32, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_TPREL64, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_BRANCH, applyJumpOrCall),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_JAL, applyJumpOrCall),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CALL, applyJumpOrCall),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CALL_PLT, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_GOT_HI20, applyGOT),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_GOT_HI20, applyGOT),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TLS_GD_HI20, applyGOT),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_PCREL_HI20, applyHI),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_PCREL_LO12_I, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_PCREL_LO12_S, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_HI20, applyHI),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_LO12_I, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_LO12_S, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TPREL_HI20, applyHI),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TPREL_LO12_I, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TPREL_LO12_S, applyLO),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_TPREL_ADD, applyTprelAdd),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_ADD8, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_ADD16, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_ADD32, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_ADD64, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB8, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB16, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB32, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB64, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_GOT32_PCREL, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_ALIGN, applyAlign),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_RVC_BRANCH, applyJumpOrCall),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_RVC_JUMP, applyJumpOrCall),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_RELAX, applyNone),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB6, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SET6, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SET8, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SET16, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SET32, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_32_PCREL, applyRel),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SET_ULEB128, applyAbs),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_SUB_ULEB128, applyAbs),

    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_VENDOR, applyVendor),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM192, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM193, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM194, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM195, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM196, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM197, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM198, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM199, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM200, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM201, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM202, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM203, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM204, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM205, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM206, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM207, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM208, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM209, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM210, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM211, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM212, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM213, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM214, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM215, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM216, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM217, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM218, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM219, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM220, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM221, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM222, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM223, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM224, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM225, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM226, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM227, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM228, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM229, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM230, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM231, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM232, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM233, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM234, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM235, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM236, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM237, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM238, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM239, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM240, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM241, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM242, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM243, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM244, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM245, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM246, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM247, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM248, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM249, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM250, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM251, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM252, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM253, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM254, unsupported),
    PUBLIC_RELOC_DESC_ENTRY(R_RISCV_CUSTOM255, unsupported),

    /* Internal Relocations for Relaxation */
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_RVC_LUI, applyCompressedLUI),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_GPREL_I, applyGPRel),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_GPREL_S, applyGPRel),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_TPREL_I, unsupported),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_TPREL_S, unsupported),

    /* Vendor Relocations: QUALCOMM */
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_QC_ABS20_U, applyAbs),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_QC_E_BRANCH, applyJumpOrCall),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_QC_E_32, applyAbs),
    INTERNAL_RELOC_DESC_ENTRY(R_RISCV_QC_E_CALL_PLT, applyJumpOrCall),
};

#undef INTERNAL_RELOC_DESC_ENTRY
#undef PUBLIC_RELOC_DESC_ENTRY

} // anonymous namespace

//===--------------------------------------------------------------------===//
// RISCVRelocator
//===--------------------------------------------------------------------===//
RISCVRelocator::RISCVRelocator(RISCVLDBackend &Backend, LinkerConfig &pConfig,
                               Module &pModule)
    : Relocator(pConfig, pModule), m_Target(Backend) {
  // Mark force verify bit for specified relcoations
  if (m_Module.getPrinter()->verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &[i, desc] : RelocDescs) {
      const auto RelocName = getRISCVRelocName(desc.type);
      if (list.find(RelocName) != list.end())
        desc.forceVerify = true;
    }
  }
}

RISCVRelocator::~RISCVRelocator() {}

namespace {

/// helper_Rela_init - Get an relocation entry in .rela.dyn
Relocation *helper_DynRel_init(ELFObjectFile *Obj, Relocation *R,
                               ResolveInfo *pSym, Fragment *F, uint32_t pOffset,
                               Relocator::Type pType, RISCVLDBackend &B) {
  Relocation *rela_entry = nullptr;

  rela_entry = Obj->getRelaDyn()->createOneReloc();
  rela_entry->setType(pType);
  rela_entry->setTargetRef(make<FragmentRef>(*F, pOffset));
  rela_entry->setSymInfo(pSym);
  if (R)
    rela_entry->setAddend(R->addend());

  // This is one insane thing, that we need to do. scanRelocations is called
  // rightly before merge sections, so any strings that are merged need to be
  // updated after merge is done to get the right symbol value. Lets record the
  // fact that we created a relative relocation for a relocation that may be
  // pointing to a merge string.
  if (R && (pType == llvm::ELF::R_RISCV_RELATIVE ||
            pType == llvm::ELF::R_RISCV_IRELATIVE)) {
    B.recordRelativeReloc(rela_entry, R);
  }
  return rela_entry;
}

RISCVGOT &CreateGOT(ELFObjectFile *Obj, Relocation &pReloc, bool pHasRel,
                    RISCVLDBackend &B, bool isExec) {
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  RISCVGOT *G = B.createGOT(GOT::Regular, Obj, rsym);

  if (!pHasRel) {
    if (!rsym->isWeakUndef())
      G->setValueType(GOT::SymbolValue);
    return *G;
  }
  uint8_t Reloc = llvm::ELF::R_RISCV_32;
  if (!B.config().targets().is32Bits())
    Reloc = llvm::ELF::R_RISCV_64;
  // If the symbol is not preemptible and we are not building an executable,
  // then try to use a relative reloc. We use a relative reloc if the symbol is
  // hidden otherwise.
  bool useRelative =
      (rsym->isHidden() || (!isExec && !B.isSymbolPreemptible(*rsym)));
  helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                     useRelative ? llvm::ELF::R_RISCV_RELATIVE : Reloc, B);
  if (useRelative) {
    G->setValueType(GOT::SymbolValue);
  }
  return *G;
}

} // namespace

Relocator::Result RISCVRelocator::applyRelocation(Relocation &pRelocation) {

  auto applyOne = [&](Relocation &pRelocation,
                      bool &hasError) -> Relocator::Result {
    Relocation::Type type = pRelocation.type();
    ResolveInfo *symInfo = pRelocation.symInfo();

    if (RelocDescs.count(type) == 0) {
      hasError = true;
      return Relocator::Unknown;
    }

    if (symInfo) {
      LDSymbol *outSymbol = symInfo->outSymbol();
      if (outSymbol && outSymbol->hasFragRef()) {
        ELFSection *S = outSymbol->fragRef()->frag()->getOwningSection();
        if (S->isDiscard() ||
            (S->getOutputSection() && S->getOutputSection()->isDiscard())) {
          std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
          issueUndefRef(pRelocation, *S->getInputFile(), S);
          hasError = true;
          return Relocator::OK;
        }
      }
    }
    return Relocator::OK;
  };

  bool err = false;

  // apply the relocation
  auto result = applyOne(pRelocation, err);
  if (err)
    return result;

  if (pRelocation.type() == llvm::ELF::R_RISCV_CALL ||
      pRelocation.type() == llvm::ELF::R_RISCV_CALL_PLT) {
    m_Target.translatePseudoRelocation(&pRelocation);
  }

  auto Desc = RelocDescs.find(pRelocation.type());
  if (Desc == RelocDescs.end())
    return RISCVRelocator::Unsupport;

  return Desc->second.func(
      pRelocation, static_cast<RISCVLDBackend &>(getTarget()), Desc->second);
}

const char *RISCVRelocator::getName(Relocation::Type pType) const {
  return getRISCVReloc(pType).Name;
}

RISCVLDBackend &RISCVRelocator::getTarget() { return m_Target; }

const RISCVLDBackend &RISCVRelocator::getTarget() const { return m_Target; }

RISCVGOT *RISCVRelocator::getTLSModuleID(ResolveInfo *R, bool isStatic) {
  static RISCVGOT *G = nullptr;
  if (G != nullptr) {
    m_Target.recordGOT(R, G);
    return G;
  }
  G = m_Target.createGOT(GOT::TLS_LD, nullptr, nullptr);
  m_Target.recordGOT(R, G);
  return G;
}

bool RISCVRelocator::isRelocSupported(Relocation &pReloc) const {
  return RelocDescs.count(pReloc.type()) != 0;
}

// Check if the relocation is invalid while generating
// dynamic libraries
bool RISCVRelocator::isInvalidReloc(Relocation &pReloc) const {
  if (!config().isCodeIndep())
    return false;

  switch (pReloc.type()) {
  case llvm::ELF::R_RISCV_HI20:
  case llvm::ELF::R_RISCV_LO12_I:
  case llvm::ELF::R_RISCV_LO12_S:
    return true;
  case llvm::ELF::R_RISCV_TPREL_HI20:
  case llvm::ELF::R_RISCV_TPREL_LO12_I:
  case llvm::ELF::R_RISCV_TPREL_LO12_S:
    return !config().options().isPIE();
  case llvm::ELF::R_RISCV_SET_ULEB128:
  case llvm::ELF::R_RISCV_SUB_ULEB128:
    return m_Target.isSymbolPreemptible(*pReloc.symInfo());
  default:
    break;
  }
  return false;
}

void RISCVRelocator::scanRelocation(Relocation &pReloc, eld::IRBuilder &pLinker,
                                    ELFSection &pSection, InputFile &pInputFile,
                                    CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  if (!isRelocSupported(pReloc)) {
    config().raise(Diag::unsupported_reloc)
        << pReloc.type() << pSection.getDecoratedName(config().options())
        << pInputFile.getInput()->decoratedPath();
    m_Target.getModule().setFailure(true);
    return;
  }

  // If we are generating a shared library check for invalid relocations
  if (isInvalidReloc(pReloc)) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    config().raise(Diag::non_pic_relocation)
        << getName(pReloc.type()) << pReloc.symInfo()->name()
        << pReloc.getSourcePath(config().options());
    m_Target.getModule().setFailure(true);
    return;
  }

  auto ProcessOneReloc = [&](Relocation &pReloc) -> void {
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

    if (rsym->isLocal()) // rsym is local
      scanLocalReloc(pInputFile, pReloc, pLinker, *section);
    else // rsym is external
      scanGlobalReloc(pInputFile, pReloc, pLinker, *section, CopyRelocs);
  };
  ProcessOneReloc(pReloc);
}

uint32_t RISCVRelocator::getNumRelocs() const {
  return (ELF::riscv::internal::LastInternalRelocation + 1);
}

Relocation::Size RISCVRelocator::getSize(Relocation::Type pType) const {
  if (RelocDescs.count(pType) == 0)
    return 0;
  return getRISCVReloc(pType).Size;
}

void RISCVRelocator::scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                                    eld::IRBuilder &pBuilder,
                                    ELFSection &pSection) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInput);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();

  // Special case when the linker makes a symbol local for example linker
  // defined symbols such as _DYNAMIC
  switch (pReloc.type()) {
  case llvm::ELF::R_RISCV_32:
  case llvm::ELF::R_RISCV_64:
    // If building PIC object (shared library or PIC executable),
    // a dynamic relocations with RELATIVE type to this location is needed.
    // Reserve an entry in .rel.dyn
    if (config().isCodeIndep()) {
      std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
      helper_DynRel_init(Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
                         pReloc.targetRef()->offset(),
                         llvm::ELF::R_RISCV_RELATIVE, m_Target);
      getTarget().checkAndSetHasTextRel(pSection);
      rsym->setReserved(rsym->reserved() | ReserveRel);
    }
    return;
  case llvm::ELF::R_RISCV_GOT_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    // If the GOT is used in statically linked binaries,
    // the GOT entry is enough and no relocation is needed.
    CreateGOT(Obj, pReloc, !config().isCodeStatic(), m_Target,
              (config().codeGenType() == LinkerConfig::Exec));
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }
  case llvm::ELF::R_RISCV_TLS_GD_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    RISCVGOT *G = m_Target.createGOT(GOT::TLS_LD, Obj, rsym);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    if (config().isCodeStatic()) {
      if (config().targets().is32Bits())
        G->getFirst()->setReservedValue(static_cast<uint32_t>(1));
      else
        G->getFirst()->setReservedValue(static_cast<uint64_t>(1));
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      G->getNext()->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    // setup dyn rel for got_entry1
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       is32bit() ? llvm::ELF::R_RISCV_TLS_DTPMOD32
                                 : llvm::ELF::R_RISCV_TLS_DTPMOD64,
                       m_Target);
    // The second slot has the symbol value (TLS Offset).
    G->getNext()->setValueType(GOT::SymbolValue);
    break;
  }
  case llvm::ELF::R_RISCV_TLS_GOT_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;
    RISCVGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    if (config().isCodeStatic()) {
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                       is32bit() ? llvm::ELF::R_RISCV_TLS_TPREL32
                                 : llvm::ELF::R_RISCV_TLS_TPREL64,
                       m_Target);
    break;
  }

  default:
    break;
  }
}

void RISCVRelocator::scanGlobalReloc(InputFile &pInputFile, Relocation &pReloc,
                                     eld::IRBuilder &pBuilder,
                                     ELFSection &pSection,
                                     CopyRelocs &CopyRelocs) {
  ELFObjectFile *Obj = llvm::dyn_cast<ELFObjectFile>(&pInputFile);
  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  RISCVLDBackend &ld_backend = getTarget();
  switch (pReloc.type()) {
  case llvm::ELF::R_RISCV_32:
  case llvm::ELF::R_RISCV_64:
  case llvm::ELF::R_RISCV_HI20:
  case llvm::ELF::R_RISCV_LO12_I:
  case llvm::ELF::R_RISCV_LO12_S:
  case eld::ELF::riscv::internal::R_RISCV_QC_E_32: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    ReservedEntryType reservedEntryType = Relocator::None;
    bool isSymbolPreemptible = m_Target.isSymbolPreemptible(*rsym);

    // Absolute relocation type, symbol may needs PLT entry or
    // dynamic relocation entry
    if ((isSymbolPreemptible ||
         (config().options().isPatchEnable() && rsym->isPatchable())) &&
        (rsym->type() == ResolveInfo::Function)) {
      // create PLT for this symbol if it does not have.
      if (!(rsym->reserved() & ReservePLT)) {
        m_Target.createPLT(Obj, rsym);
        rsym->setReserved(rsym->reserved() | ReservePLT);
      }
    }

    if (ld_backend.symbolNeedsDynRel(*rsym, (rsym->reserved() & ReservePLT),
                                     true)) {
      ResolveInfo *aliasSym = rsym->alias();
      if (ld_backend.symbolNeedsCopyReloc(pReloc, *rsym)) {
        // check if the option -z nocopyreloc is given
        if (config().options().hasNoCopyReloc()) {
          config().raise(Diag::copyrelocs_is_error)
              << rsym->name() << pInputFile.getInput()->decoratedPath()
              << rsym->resolvedOrigin()->getInput()->decoratedPath();
          return;
        }
        CopyRelocs.insert(rsym);
      } else {
        helper_DynRel_init(
            Obj, &pReloc, rsym, pReloc.targetRef()->frag(),
            pReloc.targetRef()->offset(),
            isSymbolPreemptible
                ? (is32bit() ? llvm::ELF::R_RISCV_32 : llvm::ELF::R_RISCV_64)
                : llvm::ELF::R_RISCV_RELATIVE,
            m_Target);
        reservedEntryType = Relocator::ReserveRel;
        getTarget().checkAndSetHasTextRel(pSection);
      }
      if (!aliasSym && (reservedEntryType != Relocator::None))
        rsym->setReserved(rsym->reserved() | reservedEntryType);
    }
    return;
  }

  case llvm::ELF::R_RISCV_GOT_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    // If the GOT is used in statically linked binaries,
    // the GOT entry is enough and no relocation is needed.
    if (config().isCodeStatic())
      CreateGOT(Obj, pReloc, false, m_Target,
                (config().codeGenType() == LinkerConfig::Exec));
    else
      CreateGOT(Obj, pReloc, true, m_Target,
                (config().codeGenType() == LinkerConfig::Exec));
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    return;
  }

  case llvm::ELF::R_RISCV_CALL:
  case llvm::ELF::R_RISCV_CALL_PLT:
  case eld::ELF::riscv::internal::R_RISCV_QC_E_CALL_PLT: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->reserved() & ReservePLT)
      return;
    if ((!config().isCodeStatic() && ld_backend.isSymbolPreemptible(*rsym)) ||
        (config().options().isPatchEnable() && rsym->isPatchable())) {
      m_Target.createPLT(Obj, rsym);
      rsym->setReserved(rsym->reserved() | ReservePLT);
    }
    return;
  }

  case llvm::ELF::R_RISCV_TLS_GD_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    // Symbol needs GOT entry, reserve entry in .got
    // return if we already create GOT for this symbol
    if (rsym->reserved() & ReserveGOT)
      return;
    RISCVGOT *G = m_Target.createGOT(GOT::TLS_GD, Obj, rsym);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    if (config().isCodeStatic()) {
      if (is32bit())
        G->getFirst()->setReservedValue(static_cast<uint32_t>(1));
      else
        G->getFirst()->setReservedValue(static_cast<uint64_t>(1));
      G->getFirst()->setValueType(GOT::TLSStaticSymbolValue);
      G->getNext()->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    // setup dyn rel for got entries against rsym
    helper_DynRel_init(Obj, &pReloc, rsym, G->getFirst(), 0x0,
                       is32bit() ? llvm::ELF::R_RISCV_TLS_DTPMOD32
                                 : llvm::ELF::R_RISCV_TLS_DTPMOD64,
                       m_Target);
    helper_DynRel_init(Obj, &pReloc, rsym, G->getNext(), 0x0,
                       is32bit() ? llvm::ELF::R_RISCV_TLS_DTPREL32
                                 : llvm::ELF::R_RISCV_TLS_DTPREL64,
                       m_Target);

    break;
  }

  case llvm::ELF::R_RISCV_TLS_GOT_HI20: {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (rsym->outSymbol()->type() != llvm::ELF::STT_TLS)
      config().raise(Diag::tls_non_tls_mix)
          << (int)pReloc.type() << pReloc.symInfo()->name();
    if (rsym->reserved() & ReserveGOT)
      return;
    RISCVGOT *G = m_Target.createGOT(GOT::TLS_IE, Obj, rsym);
    rsym->setReserved(rsym->reserved() | ReserveGOT);
    if (config().isCodeStatic()) {
      G->setValueType(GOT::TLSStaticSymbolValue);
      return;
    }
    helper_DynRel_init(Obj, &pReloc, rsym, G, 0x0,
                       is32bit() ? llvm::ELF::R_RISCV_TLS_TPREL32
                                 : llvm::ELF::R_RISCV_TLS_TPREL64,
                       m_Target);
    break;
  }

  default:
    break;
  }
}

void RISCVRelocator::partialScanRelocation(Relocation &pReloc,
                                           const ELFSection &pSection) {
  pReloc.updateAddend(m_Module);

  // if we meet a section symbol
  if (pReloc.symInfo()->type() == ResolveInfo::Section) {
    LDSymbol *input_sym = pReloc.symInfo()->outSymbol();

    // 1. update the relocation target offset
    assert(input_sym->hasFragRef());
    // 2. get the output ELFSection which the symbol defined in
    ELFSection *out_sect = input_sym->fragRef()->getOutputELFSection();

    ResolveInfo *sym_info = m_Module.getSectionSymbol(out_sect);
    // set relocation target symbol to the output section symbol's resolveInfo
    pReloc.setSymInfo(sym_info);
  }
}

RISCVGOT *RISCVRelocator::getTLSModuleID(ResolveInfo *R) {
  static RISCVGOT *G = nullptr;
  std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
  if (G != nullptr) {
    m_Target.recordGOT(R, G);
    return G;
  }

  // Allocate 2 got entries and 1 dynamic reloc for R_HEX_LD_GOT*
  G = m_Target.createGOT(GOT::TLS_LD, nullptr, nullptr);

  helper_DynRel_init(m_Target.getDynamicSectionHeadersInputFile(), nullptr,
                     nullptr, G, 0x0,
                     is32bit() ? llvm::ELF::R_RISCV_TLS_DTPMOD32
                               : llvm::ELF::R_RISCV_TLS_DTPMOD64,
                     m_Target);

  m_Target.recordGOT(R, G);
  return G;
}

namespace {

//=========================================//
// Relocation Verifier
//=========================================//
template <typename T>
Relocator::Result
VerifyRelocAsNeededHelper(Relocation &pReloc, T Result,
                          const RelocationDescription &pRelocDesc,
                          LinkerConfig &config) {
  uint32_t RelocType = pReloc.type();
  auto RelocInfo = getRISCVReloc(RelocType);
  Relocator::Result R = Relocator::OK;

  if ((RelocInfo.VerifyAlignment || pRelocDesc.forceVerify) &&
      !verifyRISCVAlignment(RelocInfo, Result))
    config.raise(Diag::not_aligned)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(config.options())
        << pReloc.getSourcePath(config.options()) << RelocInfo.Alignment;

  bool is32Bits = config.targets().is32Bits();
  if (RelocInfo.VerifyRange && !verifyRISCVRange(RelocInfo, Result, is32Bits))
    R = Relocator::Overflow;

  if ((pRelocDesc.forceVerify) && (isTruncatedRISCV(RelocInfo, Result))) {
    config.raise(Diag::reloc_truncated)
        << RelocInfo.Name << pReloc.symInfo()->name()
        << pReloc.getTargetPath(config.options())
        << pReloc.getSourcePath(config.options());
  }
  return R;
}

template <typename T>
Relocator::Result ApplyReloc(Relocation &pReloc, T Result,
                             const RelocationDescription &pRelocDesc,
                             LinkerConfig &config) {
  auto RelocInfo = getRISCVReloc(pReloc.type());

  // Verify the Relocation.
  Relocator::Result R = Relocator::OK;
  R = VerifyRelocAsNeededHelper(pReloc, Result, pRelocDesc, config);
  if (R != Relocator::OK)
    return R;

  // Apply the relocation.
  pReloc.target() = doRISCVReloc(RelocInfo, pReloc.target(), Result);
  return R;
}

//=========================================//
// Each relocation function implementation //
//=========================================//

RISCVRelocator::Result applyNone(Relocation &pReloc, RISCVLDBackend &,
                                 RelocationDescription &pRelocDesc) {
  return RISCVRelocator::OK;
}

// R_RISCV_[32|64]
// R_RISCV_ADD*
// R_RISCV_SUB*
RISCVRelocator::Result applyAbs(Relocation &pReloc, RISCVLDBackend &Backend,
                                RelocationDescription &pRelocDesc) {
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  // Normally, relocations are resolved to the PLT if it exists for a symbol.
  // However, relocations in the patch table must be resolved to the real
  // symbol, otherwise, they will point to themselves.
  bool IsPatchSection = pReloc.targetRef()
                            ->frag()
                            ->getOwningSection()
                            ->getInputFile()
                            ->getInput()
                            ->getAttribute()
                            .isPatchBase();
  uint64_t S = IsPatchSection ? pReloc.symValue(Backend.getModule())
                              : Backend.getSymbolValuePLT(pReloc);
  uint64_t A = pReloc.addend();
  uint64_t Result = 0;

  Relocation *groupReloc = Backend.getGroupReloc(pReloc);
  Relocation::DWord TargetData =
      groupReloc ? groupReloc->target() : pReloc.target();

  if (pReloc.type() >= llvm::ELF::R_RISCV_ADD8 &&
      pReloc.type() <= llvm::ELF::R_RISCV_ADD64)
    Result = TargetData + S + A;
  else if (pReloc.type() >= llvm::ELF::R_RISCV_SUB8 &&
           pReloc.type() <= llvm::ELF::R_RISCV_SUB64)
    Result = TargetData - (S + A);
  else if ((pReloc.type() == llvm::ELF::R_RISCV_SUB6) ||
           (pReloc.type() == llvm::ELF::R_RISCV_SUB_ULEB128))
    Result = TargetData - (S + A);
  else
    Result = S + A;

  switch (pReloc.type()) {
  case ELF::riscv::internal::R_RISCV_QC_ABS20_U:
    if (!llvm::isInt<20>(Result))
      return RISCVRelocator::Overflow;
    break;
  }

  RISCVRelocator::Result Res =
      ApplyReloc(pReloc, Result, pRelocDesc, Backend.config());
  // Update the target word for the paired reloc
  if (groupReloc) {
    groupReloc->target() = pReloc.target();
  }
  return Res;
}

RISCVRelocator::Result applyRel(Relocation &pReloc, RISCVLDBackend &Backend,
                                RelocationDescription &pRelocDesc) {
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  int64_t S = Backend.getSymbolValuePLT(pReloc);
  int64_t A = pReloc.addend();
  int64_t P = pReloc.place(Backend.getModule());

  return ApplyReloc(pReloc, S + A - P, pRelocDesc, Backend.config());
}

RISCVRelocator::Result applyHI(Relocation &pReloc, RISCVLDBackend &Backend,
                               RelocationDescription &pRelocDesc) {
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  int64_t S = Backend.getSymbolValuePLT(pReloc);
  int64_t A = pReloc.addend();
  int64_t Result = S + A + 0x800;

  if (pReloc.type() == llvm::ELF::R_RISCV_PCREL_HI20) {
    int64_t P = pReloc.place(Backend.getModule());
    bool isStaticLink = Backend.config().isCodeStatic();

    // We would like to convert the PCREL relocation to LUI
    // a. For static linkins
    //             and
    // b. If the relocation overflows PCREL
    //             and
    // c. if the relocation would fit within LUI
    if (isStaticLink && !llvm::isInt<20>((Result - P) >> 12) &&
        llvm::isInt<20>(Result >> 12)) {
      uint64_t instr = pReloc.target();
      // Convert instruction to LUI
      instr = (instr & ~0x7f) | 0x37;
      pReloc.setTargetData(instr);
      pReloc.setType(llvm::ELF::R_RISCV_HI20);
    } else {
      Result -= P;
      int wordSize = Backend.config().targets().is32Bits() ? 32 : 64;
      int64_t ResultSignExend = llvm::SignExtend64(Result, wordSize);
      // Overflow if result does not fit
      if (!llvm::isInt<20>(ResultSignExend >> 12))
        return RISCVRelocator::Overflow;
    }
  }

  return ApplyReloc(pReloc, Result, pRelocDesc, Backend.config());
}

RISCVRelocator::Result applyLO(Relocation &pReloc, RISCVLDBackend &Backend,
                               RelocationDescription &pRelocDesc) {
  DiagnosticEngine *DiagEngine = Backend.config().getDiagEngine();
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  int64_t S;
  const Relocation *HIReloc = nullptr;
  if (pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_I ||
      pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_S) {
    HIReloc = Backend.getBaseReloc(pReloc);
    if (!HIReloc)
      return RISCVRelocator::BadReloc;
    if (HIReloc->type() == llvm::ELF::R_RISCV_GOT_HI20 ||
        HIReloc->type() == llvm::ELF::R_RISCV_TLS_GD_HI20 ||
        HIReloc->type() == llvm::ELF::R_RISCV_TLS_GOT_HI20) {
      RISCVGOT *GOT = Backend.findEntryInGOT(pReloc.symInfo());
      if (!GOT)
        return RISCVRelocator::BadReloc;
      S = GOT->getAddr(DiagEngine);
    } else
      S = Backend.getSymbolValuePLT(*HIReloc);
  } else
    S = Backend.getSymbolValuePLT(pReloc);

  int64_t A = pReloc.addend();
  int64_t Result_lo = S + A;
  int64_t Result_Hi = S + A + 0x800;
  bool isStaticLink = Backend.config().isCodeStatic();
  bool isRelocDirty = false;

  if (pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_I ||
      pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_S) {
    if (HIReloc->type() == llvm::ELF::R_RISCV_HI20) {
      pReloc.setType(pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_I
                         ? llvm::ELF::R_RISCV_LO12_I
                         : llvm::ELF::R_RISCV_LO12_S);
      isRelocDirty = true;
    } else if (HIReloc->type() == llvm::ELF::R_RISCV_PCREL_HI20) {
      int64_t Result_Hi_Check =
          S + HIReloc->addend() - HIReloc->place(Backend.getModule()) + 0x800;
      if (isStaticLink && !llvm::isInt<20>(Result_Hi_Check >> 12) &&
          llvm::isInt<20>(Result_Hi >> 12)) {
        int64_t Displacement = pReloc.place(Backend.getModule()) -
                               HIReloc->place(Backend.getModule());
        Result_lo += Displacement;
        pReloc.setType(pReloc.type() == llvm::ELF::R_RISCV_PCREL_LO12_I
                           ? llvm::ELF::R_RISCV_LO12_I
                           : llvm::ELF::R_RISCV_LO12_S);
        isRelocDirty = true;
      }
    }
    if (!isRelocDirty) {
      int64_t Displacement = pReloc.place(Backend.getModule()) -
                             HIReloc->place(Backend.getModule());
      Result_lo = S + HIReloc->addend() + Displacement -
                  pReloc.place(Backend.getModule());
      Result_Hi =
          S + HIReloc->addend() - HIReloc->place(Backend.getModule()) + 0x800;
    }
  }

  switch (pReloc.type()) {
  case llvm::ELF::R_RISCV_PCREL_LO12_I:
  case llvm::ELF::R_RISCV_PCREL_LO12_S:
  case llvm::ELF::R_RISCV_TPREL_LO12_I:
  case llvm::ELF::R_RISCV_TPREL_LO12_S:
  case llvm::ELF::R_RISCV_LO12_I:
  case llvm::ELF::R_RISCV_LO12_S:
    Result_lo = Result_lo - (Result_Hi & 0xFFFFF000);
    LLVM_FALLTHROUGH;
  default:
    break;
  }
  return ApplyReloc(pReloc, Result_lo, pRelocDesc, Backend.config());
}

RISCVRelocator::Result applyGOT(Relocation &pReloc, RISCVLDBackend &Backend,
                                RelocationDescription &pRelocDesc) {
  if (!(pReloc.symInfo()->reserved() & Relocator::ReserveGOT)) {
    return Relocator::BadReloc;
  }

  int64_t S = Backend.findEntryInGOT(pReloc.symInfo())
                  ->getAddr(Backend.config().getDiagEngine());
  int64_t A = pReloc.addend();
  int64_t Result = S + A + 0x800;

  Result -= pReloc.place(Backend.getModule());

  return ApplyReloc(pReloc, Result, pRelocDesc, Backend.config());
}

RISCVRelocator::Result applyJumpOrCall(Relocation &pReloc,
                                       RISCVLDBackend &Backend,
                                       RelocationDescription &pRelocDesc) {
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  // Normally, relocations are resolved to the PLT if it exists for a symbol.
  // Direct calls can be optimized to use the real symbol.
  bool IsPatchSection = pReloc.targetRef()
                            ->frag()
                            ->getOwningSection()
                            ->getInputFile()
                            ->getInput()
                            ->getAttribute()
                            .isPatchBase();
  int64_t S = IsPatchSection ? pReloc.symValue(Backend.getModule())
                             : Backend.getSymbolValuePLT(pReloc);
  int64_t A = pReloc.addend();
  int64_t P = pReloc.place(Backend.getModule());

  return ApplyReloc(pReloc, S + A - P, pRelocDesc, Backend.config());
}

// R_RISCV_ALIGN
RISCVRelocator::Result applyAlign(Relocation &pReloc, RISCVLDBackend &Backend,
                                  RelocationDescription &pRelocDesc) {
  return RISCVRelocator::OK;
}

RISCVRelocator::Result applyGPRel(Relocation &pReloc, RISCVLDBackend &Backend,
                                  RelocationDescription &pRelocDesc) {
  if (RelocDescs.count(pReloc.type()) == 0)
    return RISCVRelocator::Unsupport;

  int64_t S = Backend.getSymbolValuePLT(pReloc);

  // Get the symbol value always from the HIRELOC.
  const Relocation *HIReloc = Backend.getBaseReloc(pReloc);
  if (HIReloc)
    S = Backend.getSymbolValuePLT(*HIReloc);

  int64_t A = pReloc.addend();
  int64_t G = 0x0;
  LDSymbol *gpSymbol =
      Backend.getModule().getNamePool().findSymbol("__global_pointer$");
  if (gpSymbol)
    G = gpSymbol->value();

  if (!llvm::isInt<12>(S + A - G))
    return RISCVRelocator::Overflow;

  return ApplyReloc(pReloc, S + A - G, pRelocDesc, Backend.config());
}

RISCVRelocator::Result applyCompressedLUI(Relocation &pReloc,
                                          RISCVLDBackend &Backend,
                                          RelocationDescription &pRelocDesc) {
  // TODO: TEst what lld/bfd does.
  // LUI has bottom 12 bits or 4K addressible target bits 0.
  uint64_t Result = Backend.getSymbolValuePLT(pReloc) + pReloc.addend();
  // The bottom 12 bits are signed.
  uint64_t LoImm = llvm::SignExtend64<12>(Result);
  return ApplyReloc(pReloc, Result - LoImm, pRelocDesc, Backend.config());
}

Relocator::Result unsupported(Relocation &pReloc, RISCVLDBackend &,
                              RelocationDescription &pRelocDesc) {
  return RISCVRelocator::Unsupport;
}

RISCVRelocator::Result applyTprelAdd(Relocation &pReloc, RISCVLDBackend &,
                                     RelocationDescription &pRelocDesc) {
  // TODO: Add support for R_RISCV_TPREL_ADD type relaxation
  return RISCVRelocator::OK;
}

// R_RISCV_VENDOR
RISCVRelocator::Result applyVendor(Relocation &pReloc, RISCVLDBackend &,
                                   RelocationDescription &pRelocDesc) {
  return RISCVRelocator::OK;
}

} // anonymous namespace

} // namespace eld
