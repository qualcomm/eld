//===- TemplateRelocator.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#include "TemplateRelocator.h"
#include "TemplateLLVMExtern.h"
#include "TemplateRelocationFunctions.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "eld/SymbolResolver/Resolver.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/DataTypes.h"

using namespace eld;

//===--------------------------------------------------------------------===//
// TemplateRelocator
//===--------------------------------------------------------------------===//
TemplateRelocator::TemplateRelocator(TemplateLDBackend &pParent,
                                     LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {
  // Mark force verify bit for specified relcoations
  if (DiagnosticPrinter::verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &i : RelocDesc) {
      auto RelocInfo = llvm::Template::Relocs[i.type];
      if (list.find(RelocInfo.Name) != list.end())
        i.forceVerify = true;
    }
  }
}

TemplateRelocator::~TemplateRelocator() {}

Relocator::Result TemplateRelocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

  ResolveInfo *symInfo = pRelocation.symInfo();

  if (symInfo) {
    LDSymbol *outSymbol = symInfo->outSymbol();
    if (outSymbol && outSymbol->hasFragRef()) {
      ELFSection *S = outSymbol->fragRef()->frag()->getOwningSection();
      if (S->kind() == LDFileFormat::Discard ||
          (S->getOutputSection() && S->getOutputSection()->isDiscard())) {
        std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
        issueUndefRef(pRelocation, *S->input(), S);
        return Relocator::OK;
      }
    }
  }

  // apply the relocation
  return RelocDesc[type].func(pRelocation, *this, RelocDesc[type]);
}

const char *TemplateRelocator::getName(Relocation::Type pType) const {
  return "";
}

void TemplateRelocator::scanRelocation(Relocation &pReloc,
                                       eld::IRBuilder &pLinker,
                                       ELFSection &pSection, Input &pInput,
                                       CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  // rsym - The relocation target symbol
  ResolveInfo *rsym = pReloc.symInfo();
  assert(nullptr != rsym &&
         "ResolveInfo of relocation not set while scanRelocation");

  // Check if we are tracing relocations.
  if (DiagnosticPrinter::traceReloc()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    std::string relocName = getName(pReloc.type());
    if (config().options().traceReloc(relocName))
      config().place(config().getDiagEngine())(diag::reloc_trace)
          << relocName << pInput.decoratedPath();
  }

  // check if we should issue undefined reference for the relocation target
  // symbol
  if (rsym->isUndef() || rsym->isBitCode()) {
    std::lock_guard<std::mutex> relocGuard(m_RelocMutex);
    if (!m_Target.canProvideSymbol(rsym)) {
      if (m_Target.canIssueUndef(rsym)) {
        if (rsym->visibility() != ResolveInfo::Default)
          issueInvisibleRef(pReloc, pInput);
        issueUndefRef(pReloc, pInput, &pSection);
      }
    }
  }

  ELFSection *section = pSection.getLink()
                            ? pSection.getLink()
                            : pReloc.targetRef()->frag()->getOwningSection();

  if (0 == (section->flag() & llvm::ELF::SHF_ALLOC))
    return;

  if (rsym->isLocal()) // rsym is local
    scanLocalReloc(pInput, pReloc, pLinker, *section);
  else // rsym is external
    scanGlobalReloc(pInput, pReloc, pLinker, *section);
}

Relocation::Size TemplateRelocator::getSize(Relocation::Type pType) const {
  return 0;
}

void TemplateRelocator::scanLocalReloc(Input &pInput, Relocation &pReloc,
                                       eld::IRBuilder &pBuilder,
                                       ELFSection &pSection) {}

void TemplateRelocator::scanGlobalReloc(Input &pInput, Relocation &pReloc,
                                        eld::IRBuilder &pBuilder,
                                        ELFSection &pSection) {}

void TemplateRelocator::partialScanRelocation(Relocation &pReloc,
                                              const ELFSection &pSection) {
  pReloc.updateAddend(config().getDiagEngine());

  // if we meet a section symbol
  if (pReloc.symInfo()->type() == ResolveInfo::Section) {
    LDSymbol *input_sym = pReloc.symInfo()->outSymbol();

    // 1. update the relocation target offset
    assert(input_sym->hasFragRef());
    // 2. get the output ELFSection which the symbol defined in
    ELFSection *out_sect = input_sym->fragRef()->frag()->getOutputSection();

    ResolveInfo *sym_info = m_Module.getSectionSymbol(out_sect);
    // set relocation target symbol to the output section symbol's resolveInfo
    pReloc.setSymInfo(sym_info);
  }
}

//=========================================//
// Relocation Verifier
//=========================================//
template <typename T>
Relocator::Result
VerifyRelocAsNeededHelper(Relocation &pReloc, T Result,
                          const RelocationDescription &pRelocDesc) {
  Relocator::Result R = Relocator::OK;
  return R;
}

Relocator::Result ApplyReloc(Relocation &pReloc, uint32_t Result,
                             const RelocationDescription &pRelocDesc) {

  // Verify the Relocation.
  Relocator::Result R = Relocator::OK;
  return R;
}

//=========================================//
// Each relocation function implementation //
//=========================================//

TemplateRelocator::Result eld::applyNone(Relocation &pReloc,
                                         TemplateRelocator &pParent,
                                         RelocationDescription &pRelocDesc) {
  return TemplateRelocator::OK;
}

TemplateVRelocator::Result eld::applyAbs(Relocation &pReloc,
                                         TemplateRelocator &pParent,
                                         RelocationDescription &pRelocDesc) {
  if (pReloc.type() >= TEMPLATE_MAXRELOCS)
    return TemplateRelocator::Unsupport;

  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();

  return ApplyReloc(pReloc, S + A, pRelocDesc);
}

TemplateRelocator::Result eld::applyRel(Relocation &pReloc,
                                        TemplateRelocator &pParent,
                                        RelocationDescription &pRelocDesc) {

  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();
  int64_t P = pReloc.place(config().getDiagEngine());

  return ApplyReloc(pReloc, S + A - P, pRelocDesc);
}

TemplateRelocator::Result eld::applyHILO(Relocation &pReloc,
                                         TemplateRelocator &pParent,
                                         RelocationDescription &pRelocDesc) {
  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();

  return ApplyReloc(pReloc, S + A, pRelocDesc);
}

TemplateRelocator::Result eld::applyRelax(Relocation &pReloc,
                                          TemplateRelocator &pParent,
                                          RelocationDescription &pRelocDesc) {

  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();

  return ApplyReloc(pReloc, S + A, pRelocDesc);
}

TemplateRelocator::Result
eld::applyJumpOrCall(Relocation &pReloc, TemplateRelocator &pParent,
                     RelocationDescription &pRelocDesc) {

  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();

  return ApplyReloc(pReloc, S + A, pRelocDesc);
}

TemplateRelocator::Result eld::applyAlign(Relocation &pReloc,
                                          TemplateRelocator &pParent,
                                          RelocationDescription &pRelocDesc) {

  int64_t S = pReloc.symValue();
  int64_t A = pReloc.addend();

  return ApplyReloc(pReloc, S + A, pRelocDesc);
}

TemplateRelocator::Result eld::applyGPRel(Relocation &pReloc,
                                          TemplateRelocator &pParent,
                                          RelocationDescription &pRelocDesc)

    int64_t S = pReloc.symValue();
int64_t A = pReloc.addend();

return ApplyReloc(pReloc, S + A, pRelocDesc);
}

Relocator::Result eld::unsupported(Relocation &pReloc,
                                   TemplateRelocator &pParent,
                                   RelocationDescription &pRelocDesc) {
  return TemplateRelocator::Unsupport;
}
