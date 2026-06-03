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
  // Mark force verify bit for specified relocations
  if (m_Module.getPrinter()->verifyReloc() &&
      config().options().verifyRelocList().size()) {
    auto &list = config().options().verifyRelocList();
    for (auto &i : RelocDesc) {
      auto RelocInfo = llvm::Template::Relocs[i.type];
      if (list.find(RelocInfo.Name) != list.end())
        i.forceVerify = true;
    }
  }
}

Relocator::Result TemplateRelocator::applyRelocation(Relocation &pRelocation) {
  Relocation::Type type = pRelocation.type();

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

  // apply the relocation
  return RelocDesc[type].func(pRelocation, *this, RelocDesc[type]);
}

const char *TemplateRelocator::getName(Relocation::Type pType) const {
  return "";
}

void TemplateRelocator::scanRelocation(Relocation &pReloc,
                                       eld::IRBuilder &pLinker,
                                       ELFSection &pSection,
                                       InputFile &pInputFile,
                                       CopyRelocs &CopyRelocs) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

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
    scanGlobalReloc(pInputFile, pReloc, pLinker, *section);
}

Relocation::Size TemplateRelocator::getSize(Relocation::Type pType) const {
  return 0;
}

void TemplateRelocator::scanLocalReloc(InputFile &pInput, Relocation &pReloc,
                                       eld::IRBuilder &pBuilder,
                                       ELFSection &pSection) {}

void TemplateRelocator::scanGlobalReloc(InputFile &pInput, Relocation &pReloc,
                                        eld::IRBuilder &pBuilder,
                                        ELFSection &pSection) {}

void TemplateRelocator::partialScanRelocation(Relocation &pReloc,
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

uint32_t TemplateRelocator::getNumRelocs() const { return TEMPLATE_MAXRELOCS; }

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

TemplateRelocator::Result applyNone(Relocation &pReloc,
                                    TemplateRelocator &pParent,
                                    RelocationDescription &pRelocDesc) {
  return TemplateRelocator::OK;
}

Relocator::Result unsupported(Relocation &pReloc, TemplateRelocator &pParent,
                              RelocationDescription &pRelocDesc) {
  return TemplateRelocator::Unsupport;
}
