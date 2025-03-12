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

ScriptMemoryRegion::ScriptMemoryRegion(MemoryDesc *Desc) : m_MemoryDesc(Desc) {}

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
  m_OutputSections.push_back(O);
  if (getSize() > getLength().value())
    FirstOutputSectionExceededLimit = O;
}

eld::Expected<void>
ScriptMemoryRegion::verifyMemoryUsage(LinkerConfig &config) {
  if (FirstOutputSectionExceededLimit)
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_memory_region_exceeded_limit,
        {getName(), std::string(FirstOutputSectionExceededLimit->name())}));
  if (!getSize() && config.showLinkerScriptMemoryWarnings())
    config.raise(diag::warn_memory_region_zero_sized) << getName();
  config.raise(diag::verbose_verified_add_memory_region) << getName();
  return eld::Expected<void>();
}

eld::Expected<uint64_t> ScriptMemoryRegion::getOrigin() const {
  MemorySpec *spec = m_MemoryDesc->getMemorySpec();
  Expression *Origin = spec->getOrigin();
  return Origin->evaluateAndReturnError();
}

eld::Expected<uint64_t> ScriptMemoryRegion::getLength() const {
  MemorySpec *spec = m_MemoryDesc->getMemorySpec();
  Expression *Length = spec->getLength();
  return Length->evaluateAndReturnError();
}

size_t ScriptMemoryRegion::getSize() const {
  size_t Sz = 0;
  if (!m_OutputSections.size())
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
  MemorySpec *spec = m_MemoryDesc->getMemorySpec();
  std::string Attributes = spec->getMemoryAttributes();
  // Nothing to do
  if (Attributes.empty())
    return eld::Expected<void>();
  bool foundInvertedFlags = false;
  char prev = 0;
  for (const auto &c : Attributes) {
    char attr = tolower(c);
    switch (attr) {
    case '(':
    case ')':
      break;
    case '!': {
      if (foundInvertedFlags && prev == '!')
        return std::make_unique<plugin::DiagnosticEntry>(
            plugin::DiagnosticEntry(diag::error_inverted_allowed_only_once,
                                    {getName()}));
      std::swap(AttrFlags, AttrNegFlags);
      std::swap(AttrInvertedFlags, AttrInvertedNegFlags);
      foundInvertedFlags = !foundInvertedFlags;
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
    prev = c;
  }
  if (foundInvertedFlags && (AttrFlags || AttrNegFlags)) {
    std::swap(AttrFlags, AttrNegFlags);
    std::swap(AttrInvertedFlags, AttrInvertedNegFlags);
  }
  if (foundInvertedFlags && (!AttrInvertedFlags && !AttrNegFlags)) {
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_no_inverted_flags_present, {getName()}));
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
  MemorySpec *spec = m_MemoryDesc->getMemorySpec();
  O->epilog().setRegion(this, spec->getMemoryDescriptorToken());
  if (!O->epilog().hasLMARegion() && !O->prolog().hasLMA())
    O->epilog().setLMARegion(this, spec->getMemoryDescriptorToken());
  return true;
}

eld::Expected<void>
ScriptMemoryRegion::printMemoryUsage(llvm::raw_ostream &os) const {
  auto printSize = [&](uint64_t size) {
    if ((size & 0x3fffffff) == 0)
      os << llvm::format_decimal(size >> 30, 10) << " GB";
    else if ((size & 0xfffff) == 0)
      os << llvm::format_decimal(size >> 20, 10) << " MB";
    else if ((size & 0x3ff) == 0)
      os << llvm::format_decimal(size >> 10, 10) << " KB";
    else
      os << " " << llvm::format_decimal(size, 10) << " B";
  };
  auto Length = getLength();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(Length);
  uint64_t Size = getSize();
  os << llvm::right_justify(getName(), 16);
  os << ": ";
  printSize(Size);
  uint64_t length = Length.value();
  if (length != 0) {
    printSize(length);
    double percent = Size * 100.0 / length;
    os << "    " << llvm::format("%6.2f%%", percent);
  }
  os << '\n';
  return eld::Expected<void>();
}

void ScriptMemoryRegion::printHeaderForMemoryUsage(llvm::raw_ostream &os) {
  os << "Memory region         Used Size  Region Size  %age Used\n";
}

std::string ScriptMemoryRegion::getName() const {
  return getMemoryDesc()->getMemorySpec()->getMemoryDescriptor();
}

bool ScriptMemoryRegion::containsVMA(uint64_t addr) const {
  uint64_t Origin = getOrigin().value();
  uint64_t Length = getLength().value();
  return (addr >= Origin) && (addr <= Origin + Length);
}
