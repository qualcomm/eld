//===- LayoutWrapper.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/PluginAPI/LayoutWrapper.h"
#include "eld/Core/Module.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Target/GNULDBackend.h"

using namespace eld;
using namespace eld::plugin;

LayoutWrapper::LayoutWrapper(const LinkerWrapper &Linker) : Linker(Linker) {}

const MapHeader LayoutWrapper::getMapHeader() const {
  return MapHeader(Linker.getLinkerConfig());
}

uint32_t LayoutWrapper::getABIPageSize() const {
  return Linker.getModule()->getBackend()->abiPageSize();
}

std::string LayoutWrapper::getTargetEmulation() const {
  std::string flagString = "";
  flagString = Linker.getModule()->getBackend()->getInfo().flagString(
      Linker.getModule()->getBackend()->getInfo().flags());
  return flagString;
}

void LayoutWrapper::recordPadding(std::vector<Padding> &Paddings,
                                  uint64_t startOffset, uint64_t size,
                                  uint64_t fillValue, bool isAlignment) {
  Padding padding;
  padding.startOffset = startOffset;
  padding.size = size;
  padding.fillValue = fillValue;
  padding.isAlignment = isAlignment;
  Paddings.push_back(padding);
}

std::vector<Padding>
LayoutWrapper::getPaddings(eld::plugin::OutputSection &Section) {
  std::vector<Padding> paddings;
  eld::OutputSectionEntry *entry = Section.getOutputSection();
  // padding at section start
  for (const eld::GNULDBackend::PaddingT &p :
       Linker.getModule()->getBackend()->getPaddingBetweenFragments(
           entry->getSection(), nullptr, entry->getFirstFrag())) {
    if (!(p.endOffset - p.startOffset))
      continue;
    recordPadding(paddings, p.startOffset, p.endOffset - p.startOffset,
                  p.Exp ? p.Exp->result() : 0, /*isAlignment*/ false);
  }
  for (eld::OutputSectionEntry::iterator it = entry->begin(),
                                         itEnd = entry->end();
       it != itEnd; ++it) {
    eld::ELFSection *section = (*it)->getSection();
    if (!section || !section->getFragmentList().size())
      continue;
    // fragment padding
    for (auto &F : (*it)->getSection()->getFragmentList()) {
      if (!F->size())
        continue;
      if (!F->paddingSize())
        continue;
      eld::DiagnosticEngine *DiagEngine =
          Linker.getModule()->getConfig().getDiagEngine();
      std::optional<uint64_t> paddingValue =
          Linker.getModule()->getFragmentPaddingValue(F);
      recordPadding(paddings, F->getOffset(DiagEngine) - F->paddingSize(),
                    F->paddingSize(), paddingValue ? *paddingValue : 0,
                    /*isAlignment*/ true);
    }
    // padding between rules
    const eld::RuleContainer *NextRuleWithContent =
        (*it)->getNextRuleWithContent();
    if ((*it)->hasContent()) {
      for (const eld::GNULDBackend::PaddingT &p :
           Linker.getModule()->getBackend()->getPaddingBetweenFragments(
               entry->getSection(), (*it)->getLastFrag(),
               NextRuleWithContent ? NextRuleWithContent->getFirstFrag()
                                   : nullptr)) {
        recordPadding(paddings, p.startOffset, p.endOffset - p.startOffset,
                      p.Exp ? p.Exp->result() : 0, /*isAlignment*/ false);
      }
    }
  }
  return paddings;
}

LayoutWrapper::~LayoutWrapper() {}
