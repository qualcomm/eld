//===- X86_32Relocator.cpp-----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
// i386 uses Elf32_Rel records: there is no r_addend field, so the addend is
// read from (and the result is written back into) the relocation's target
// field.
//===----------------------------------------------------------------------===//

#include "X86_32Relocator.h"
#include "X86_32RelocationFunctions.h"
#include "X86_32RelocationInfo.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/SymbolResolver/LDSymbol.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/MathExtras.h"

namespace eld {
namespace {

bool isSupportedRelocation(Relocation::Type Type) {
  return Type < x86_32::MaxRelocs &&
         x86_32::RelocDesc[Type] != &x86_32::unsupported;
}

// i386 is SHT_REL: the addend is stored in the target field itself, sign
// extended from the width of the field it was read from.
Relocator::DWord getImplicitAddend(const Relocation &pReloc) {
  const x86_32::RelocationInfo &Info = x86_32::Relocs[pReloc.type()];
  const unsigned Bits = x86_32::getFieldBits(Info.Width);
  return llvm::SignExtend64(pReloc.target(), Bits) + pReloc.addend();
}

// Range-check the computed relocation value and, if it fits, encode it into
// the low bits of the relocation's target field.
Relocator::Result applyRelocationValue(Relocation &pReloc, uint64_t Value,
                                       X86_32Relocator &Parent) {
  const x86_32::RelocationInfo &Info = x86_32::Relocs[pReloc.type()];
  const unsigned Bits = x86_32::getFieldBits(Info.Width);
  // ELF32 relocation expressions wrap at 32 bits before narrow-field checks.
  const uint32_t Value32 = static_cast<uint32_t>(Value);
  const int64_t SignedValue = llvm::SignExtend64(Value32, 32);

  if (Info.Range == x86_32::RangeCheck::SignedOrUnsigned) {
    if (!llvm::isIntN(Bits, SignedValue) &&
        !llvm::isUIntN(Bits, static_cast<uint64_t>(SignedValue))) {
      pReloc.issueSignedOverflow(Parent, SignedValue, llvm::minIntN(Bits),
                                 llvm::maxUIntN(Bits));
      return Relocator::Overflow;
    }
  }

  const uint32_t Mask = Bits == 32 ? 0xFFFFFFFFU : (1U << Bits) - 1;
  pReloc.target() =
      (static_cast<uint32_t>(pReloc.target()) & ~Mask) | (Value32 & Mask);
  return Relocator::OK;
}

} // namespace

//===--------------------------------------------------------------------===//
// X86_32Relocator
//===--------------------------------------------------------------------===//
X86_32Relocator::X86_32Relocator(X86_32LDBackend &pParent,
                                 LinkerConfig &pConfig, Module &pModule)
    : Relocator(pConfig, pModule), m_Target(pParent) {}

Relocator::Result X86_32Relocator::applyRelocation(Relocation &pRelocation) {
  const Relocation::Type Type = pRelocation.type();
  if (Type >= x86_32::MaxRelocs)
    return Relocator::Unknown;

  ResolveInfo *SymInfo = pRelocation.symInfo();
  if (SymInfo) {
    LDSymbol *OutSymbol = SymInfo->outSymbol();
    if (OutSymbol && OutSymbol->hasFragRef()) {
      ELFSection *Section = OutSymbol->fragRef()->frag()->getOwningSection();
      if (Section->isDiscard() || (Section->getOutputSection() &&
                                   Section->getOutputSection()->isDiscard())) {
        std::lock_guard<std::mutex> RelocGuard(m_RelocMutex);
        issueUndefRef(pRelocation, *Section->getInputFile(), Section);
        return Relocator::OK;
      }
    }
  }

  return x86_32::RelocDesc[Type](pRelocation, *this);
}

void X86_32Relocator::scanRelocation(Relocation &pReloc, eld::IRBuilder &,
                                     ELFSection &pSection,
                                     InputFile &pInputFile, CopyRelocs &) {
  if (LinkerConfig::Object == config().codeGenType())
    return;

  if (!isSupportedRelocation(pReloc.type())) {
    config().raise(Diag::unsupported_reloc)
        << pReloc.type() << pSection.getDecoratedName(config().options())
        << pInputFile.getInput()->decoratedPath();
    return;
  }

  ResolveInfo *SymInfo = pReloc.symInfo();
  assert(SymInfo != nullptr &&
         "ResolveInfo of relocation not set while scanRelocation");

  if (m_Module.getPrinter()->traceReloc()) {
    std::lock_guard<std::mutex> RelocGuard(m_RelocMutex);
    const std::string RelocName = getName(pReloc.type());
    if (config().options().traceReloc(RelocName))
      config().raise(Diag::reloc_trace)
          << RelocName << SymInfo->name()
          << pInputFile.getInput()->decoratedPath();
  }

  if (SymInfo->isUndef() || SymInfo->isBitCode()) {
    std::lock_guard<std::mutex> RelocGuard(m_RelocMutex);
    if (m_Target.canIssueUndef(SymInfo)) {
      if (SymInfo->visibility() != ResolveInfo::Default)
        issueInvisibleRef(pReloc, pInputFile);
      issueUndefRef(pReloc, pInputFile, &pSection);
    }
  }
}

const char *X86_32Relocator::getName(Relocation::Type pType) const {
  if (pType >= x86_32::MaxRelocs)
    return "INVALID_RELOC";
  return x86_32::Relocs[pType].Name;
}

Relocation::Size X86_32Relocator::getSize(Relocation::Type pType) const {
  if (pType >= x86_32::MaxRelocs)
    return 0;
  return x86_32::getFieldBits(x86_32::Relocs[pType].Width);
}

uint32_t X86_32Relocator::getNumRelocs() const { return x86_32::MaxRelocs; }

//===--------------------------------------------------------------------===//
// Relocation handlers
//===--------------------------------------------------------------------===//
namespace x86_32 {

Relocator::Result none(Relocation &, X86_32Relocator &) {
  return Relocator::OK;
}

// R_386_8, R_386_16, R_386_32: S + A.
Relocator::Result relocAbs(Relocation &pReloc, X86_32Relocator &pParent) {
  Relocator::Address Symbol = pReloc.symValue(pParent.module());
  const Relocator::DWord Addend = getImplicitAddend(pReloc);
  ResolveInfo *SymInfo = pReloc.symInfo();

  // A weak undefined symbol has value zero in a static executable.
  if (SymInfo && SymInfo->isWeakUndef() &&
      pParent.config().codeGenType() == LinkerConfig::Exec)
    Symbol = 0;

  return applyRelocationValue(pReloc, Symbol + Addend, pParent);
}

Relocator::Result unsupported(Relocation &, X86_32Relocator &) {
  return Relocator::Unsupport;
}

} // namespace x86_32
} // namespace eld
