//===- ScriptMemoryRegion.cpp----------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Object/ScriptMemoryRegion.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Object/OutputSectionEntry.h"
#include "eld/Script/Expression.h"
#include "llvm/Support/Format.h"

using namespace eld;

ScriptMemoryRegion::ScriptMemoryRegion(MemoryDesc *Desc) : MMemoryDesc(Desc) {}

void ScriptMemoryRegion::addOutputSectionVMA(const OutputSectionEntry *O) {
  CurrentCursor = O->getSection()->addr();
  addOutputSection(O);
}

void ScriptMemoryRegion::addOutputSectionLMA(const OutputSectionEntry *O) {
  CurrentCursor = O->getSection()->pAddr();
  addOutputSection(O);
}

void ScriptMemoryRegion::addOutputSection(const OutputSectionEntry *O) {
  const ELFSection *S = O->getSection();
  if (!S->isTBSS())
    CurrentCursor = CurrentCursor.value() + S->size();
  MOutputSections.push_back(O);
  if (getSize() > getLength().value())
    FirstOutputSectionExceededLimit = O;
}

eld::Expected<void>
ScriptMemoryRegion::verifyMemoryUsage(LinkerConfig &Config) {
  if (FirstOutputSectionExceededLimit)
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_memory_region_exceeded_limit,
        {getName(), std::string(FirstOutputSectionExceededLimit->name())}));
  if (!getSize() && Config.showLinkerScriptMemoryWarnings())
    Config.raise(Diag::warn_memory_region_zero_sized) << getName();
  Config.raise(Diag::verbose_verified_add_memory_region) << getName();
  return eld::Expected<void>();
}

eld::Expected<uint64_t> ScriptMemoryRegion::getOrigin() const {
  MemorySpec *Spec = MMemoryDesc->getMemorySpec();
  Expression *Origin = Spec->getOrigin();
  return Origin->evaluateAndReturnError();
}

eld::Expected<uint64_t> ScriptMemoryRegion::getLength() const {
  MemorySpec *Spec = MMemoryDesc->getMemorySpec();
  Expression *Length = Spec->getLength();
  return Length->evaluateAndReturnError();
}

size_t ScriptMemoryRegion::getSize() const {
  size_t Sz = 0;
  if (!MOutputSections.size())
    return Sz;
  Sz = CurrentCursor.value() - getOrigin().value();
  return Sz;
}

uint64_t ScriptMemoryRegion::getAddr() {
  if (!CurrentCursor)
    CurrentCursor = getOrigin().value();
  return CurrentCursor.value();
}

// This function parses the attributes used to match against section
// flags when placing output sections in a memory region. These flags
// are only used when an explicit memory region name is not used.
eld::Expected<void> ScriptMemoryRegion::parseMemoryAttributes() {
  MemorySpec *Spec = MMemoryDesc->getMemorySpec();
  std::string Attributes = Spec->getMemoryAttributes();
  // Nothing to do
  if (Attributes.empty())
    return eld::Expected<void>();
  bool FoundInvertedFlags = false;
  char Prev = 0;
  for (const auto &C : Attributes) {
    char Attr = tolower(C);
    switch (Attr) {
    case '(':
    case ')':
      break;
    case '!': {
      if (FoundInvertedFlags && Prev == '!')
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(Diag::error_inverted_allowed_only_once,
                                    {getName()}));
      std::swap(AttrFlags, AttrNegFlags);
      std::swap(AttrInvertedFlags, AttrInvertedNegFlags);
      FoundInvertedFlags = !FoundInvertedFlags;
      break;
    }
    case 'r':
      AttrInvertedFlags |= ScriptMemoryRegion::MemoryAttributes::Write;
      break;
    case 'w':
      AttrFlags |= ScriptMemoryRegion::MemoryAttributes::Write;
      break;
    case 'x':
      AttrFlags |= ScriptMemoryRegion::MemoryAttributes::Execute;
      break;
    case 'a':
      AttrFlags |= ScriptMemoryRegion::MemoryAttributes::Alloc;
      break;
    case 'i':
    case 'l':
      AttrFlags |= ScriptMemoryRegion::MemoryAttributes::Progbits;
      break;
    default:
      break;
    }
    Prev = C;
  }
  if (FoundInvertedFlags && (AttrFlags || AttrNegFlags)) {
    std::swap(AttrFlags, AttrNegFlags);
    std::swap(AttrInvertedFlags, AttrInvertedNegFlags);
  }
  if (FoundInvertedFlags && (!AttrInvertedFlags && !AttrNegFlags)) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_no_inverted_flags_present, {getName()}));
  }
  return eld::Expected<void>();
}

bool ScriptMemoryRegion::isCompatible(const ELFSection *S) const {
  uint32_t MatchFlag = 0;
  if (S->isAlloc())
    MatchFlag |= ScriptMemoryRegion::MemoryAttributes::Alloc;
  if (S->isWritable())
    MatchFlag |= ScriptMemoryRegion::MemoryAttributes::Write;
  if (S->isCode())
    MatchFlag |= ScriptMemoryRegion::MemoryAttributes::Execute;
  if (S->isProgBits())
    MatchFlag |= ScriptMemoryRegion::MemoryAttributes::Progbits;
  if ((MatchFlag & AttrNegFlags) || (~MatchFlag & AttrInvertedNegFlags))
    return false;
  return (MatchFlag & AttrFlags) || (~MatchFlag & AttrInvertedFlags);
}

bool ScriptMemoryRegion::checkCompatibilityAndAssignMemorySpecToOutputSection(
    OutputSectionEntry *O) {
  // No need to override the region if the region is alrady there
  if (O->epilog().hasRegion())
    return true;
  ELFSection *S = O->getSection();
  if (!S->isAlloc())
    return true;
  if (!isCompatible(S))
    return false;
  MemorySpec *Spec = MMemoryDesc->getMemorySpec();
  O->epilog().setRegion(this, Spec->getMemoryDescriptorToken());
  if (!O->epilog().hasLMARegion() && !O->prolog().hasLMA())
    O->epilog().setLMARegion(this, Spec->getMemoryDescriptorToken());
  return true;
}

eld::Expected<void>
ScriptMemoryRegion::printMemoryUsage(llvm::raw_ostream &Os) const {
  auto PrintSize = [&](uint64_t Size) {
    if ((Size & 0x3fffffff) == 0)
      Os << llvm::format_decimal(Size >> 30, 10) << " GB";
    else if ((Size & 0xfffff) == 0)
      Os << llvm::format_decimal(Size >> 20, 10) << " MB";
    else if ((Size & 0x3ff) == 0)
      Os << llvm::format_decimal(Size >> 10, 10) << " KB";
    else
      Os << " " << llvm::format_decimal(Size, 10) << " B";
  };
  auto MemoryLength = getLength();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(MemoryLength);
  uint64_t Size = getSize();
  Os << llvm::right_justify(getName(), 16);
  Os << ": ";
  PrintSize(Size);
  uint64_t Length = MemoryLength.value();
  if (Length != 0) {
    PrintSize(Length);
    double Percent = Size * 100.0 / Length;
    Os << "    " << llvm::format("%6.2f%%", Percent);
  }
  Os << '\n';
  return eld::Expected<void>();
}

void ScriptMemoryRegion::printHeaderForMemoryUsage(llvm::raw_ostream &Os) {
  Os << "Memory region         Used Size  Region Size  %age Used\n";
}

std::string ScriptMemoryRegion::getName() const {
  return getMemoryDesc()->getMemorySpec()->getMemoryDescriptor();
}

bool ScriptMemoryRegion::containsVMA(uint64_t Addr) const {
  uint64_t Origin = getOrigin().value();
  uint64_t Length = getLength().value();
  return (Addr >= Origin) && (Addr <= Origin + Length);
}
