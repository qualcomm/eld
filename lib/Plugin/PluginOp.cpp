//===- PluginOp.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Plugin/PluginOp.h"
#include "eld/Object/SectionMap.h"
#include "eld/PluginAPI/LinkerWrapper.h"
#include "eld/Readers/ELFSection.h"

using namespace eld;

PluginOp::PluginOp(plugin::LinkerWrapper *W, PluginOpType T,
                   std::string Annotation)
    : Wrapper(W), OpType(T), Annotation(Annotation) {}

std::string PluginOp::getPluginName() const {
  return Wrapper->getPlugin()->getPluginName();
}

ChangeOutputSectionPluginOp::ChangeOutputSectionPluginOp(
    plugin::LinkerWrapper *W, eld::ELFSection *S, std::string OutputSection,
    std::string Annotation)
    : PluginOp(W, PluginOp::ChangeOutputSection, Annotation),
      OrigRule(S->getMatchedLinkerScriptRule()), ESection(S),
      OutputSection(OutputSection) {}

AddChunkPluginOp::AddChunkPluginOp(plugin::LinkerWrapper *W,
                                   RuleContainer *Rule, eld::Fragment *Frag,
                                   std::string Annotation)
    : PluginOp(W, PluginOp::AddChunk, Annotation), Rule(Rule), Frag(Frag) {}

RemoveChunkPluginOp::RemoveChunkPluginOp(plugin::LinkerWrapper *W,
                                         RuleContainer *Rule,
                                         eld::Fragment *Frag,
                                         std::string Annotation)
    : PluginOp(W, PluginOp::RemoveChunk, Annotation), Rule(Rule), Frag(Frag) {}

UpdateChunksPluginOp::UpdateChunksPluginOp(plugin::LinkerWrapper *W,
                                           RuleContainer *Rule, Type T,
                                           std::string Annotation)
    : PluginOp(W, PluginOp::UpdateChunks, Annotation), Rule(Rule), T(T) {}

RemoveSymbolPluginOp::RemoveSymbolPluginOp(plugin::LinkerWrapper *W,
                                           std::string Annotation,
                                           const ResolveInfo *S)
    : PluginOp(W, PluginOp::RemoveSymbol, Annotation), RemovedSymbol(S) {}

RelocationDataPluginOp::RelocationDataPluginOp(plugin::LinkerWrapper *W,
                                               const eld::Relocation *R,
                                               const std::string &Annotation)
    : PluginOp(W, PluginOp::RelocationData, Annotation), Relocation(R) {}

const eld::Fragment *RelocationDataPluginOp::getFrag() const {
  return Relocation->targetRef()->frag();
}

ResetOffsetPluginOp::ResetOffsetPluginOp(plugin::LinkerWrapper *W,
                                         const OutputSectionEntry *O,
                                         uint32_t OldOffset,
                                         const std::string &Annotation)
    : PluginOp(W, PluginOp::ResetOffset, Annotation), O(O),
      OldOffset(OldOffset) {}
