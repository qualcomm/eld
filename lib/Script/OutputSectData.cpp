//===- OutputSectData.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/OutputSectData.h"
#include "eld/Config/Version.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/OutputSectDataFragment.h"
#include "eld/Fragment/StringFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/LayoutMap/LayoutInfo.h"
#include "eld/Object/RuleContainer.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Script/Expression.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Target/LDFileFormat.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>

using namespace eld;

namespace {

std::string buildLinkerVersionString() {
  std::string Version = "eld ";
  Version += eld::getELDVersion();
  Version += " (GNU Compatible linker)";
  return Version;
}

} // namespace

OutputSectData *OutputSectData::create(uint32_t ID, OutputSectDesc &OutSectDesc,
                                       OSDKind Kind, Expression &Expr) {
  InputSectDesc::Spec Spec;
  Spec.initialize();
  InputSectDesc::Policy Policy = InputSectDesc::Policy::Keep;
  OutputSectData *OSD =
      eld::make<OutputSectData>(ID, Policy, Spec, OutSectDesc, Kind, Expr);
  return OSD;
}

OutputSectData::OutputSectData(uint32_t ID, InputSectDesc::Policy Policy,
                               const InputSectDesc::Spec Spec,
                               OutputSectDesc &OutSectDesc, OSDKind Kind,
                               Expression &Expr)
    : InputSectDesc(ScriptCommand::Kind::OUTPUT_SECT_DATA, ID, Policy, Spec,
                    OutSectDesc),
      MOsdKind(Kind), ExpressionToEvaluate(Expr) {}

llvm::StringRef OutputSectData::getOSDKindAsStr() const {
#define ADD_CASE(dataKind)                                                     \
  case OSDKind::dataKind:                                                      \
    return #dataKind;
  switch (MOsdKind) {
    ADD_CASE(None);
    ADD_CASE(Byte);
    ADD_CASE(Short);
    ADD_CASE(Long);
    ADD_CASE(Quad);
    ADD_CASE(Squad);
  }
#undef ADD_CASE
}

std::size_t OutputSectData::getDataSize() const {
  switch (MOsdKind) {
  case Byte:
    return 1;
  case Short:
    return 2;
  case Long:
    return 4;
  case Quad:
  case Squad:
    return 8;
  default:
    llvm_unreachable(
        (llvm::Twine("Invalid output section data: ") + getOSDKindAsStr())
            .str()
            .c_str());
  }
}

eld::Expected<void> OutputSectData::activate(Module &Module) {
  ExpressionToEvaluate.setContext(getContext());
  std::pair<SectionMap::mapping, bool> Mapping =
      Module.getScript().sectionMap().insert(*this, OutputSectionDescription);
  ASSERT(Mapping.second,
         "New rule must be created for each output section data!");
  ThisRuleContainer = Mapping.first.second;

  ThisSectionion = createOSDSection(Module);

  // FIXME: We need to perform the below steps whenever we associate
  // a rule with a section. We can perhaps simplify this process by
  // creating a utility function which performs the below steps.
  RuleContainer *R = ThisRuleContainer;
  ThisSectionion->setOutputSection(R->getSection()->getOutputSection());
  R->incMatchCount();
  ThisSectionion->setMatchedLinkerScriptRule(R);
  return eld::Expected<void>();
}

ELFSection *OutputSectData::createOSDSection(Module &Module) {
  ASSERT(ThisRuleContainer, "Rule container must be set before the output data "
                            "section can be created!");
  RuleContainer *R = ThisRuleContainer;

  llvm::StringRef OutputSectName = R->getSection()->getOutputSection()->name();
  std::string Name = "__OutputSectData." + OutputSectName.str() + "." +
                     getOSDKindAsStr().str();
  ELFSection *S = Module.createInternalSection(
      Module::InternalInputType::OutputSectData, LDFileFormat::OutputSectData,
      Name, DefaultSectionType, DefaultSectionFlags, /*alignment=*/1);
  Fragment *F = make<OutputSectDataFragment>(*this);
  LayoutInfo *layoutInfo = Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(
        Module.getInternalInput(Module::InternalInputType::OutputSectData), S,
        F);
  S->addFragmentAndUpdateSize(F);
  return S;
}

void OutputSectData::dump(llvm::raw_ostream &Outs) const {
  return dumpMap(Outs);
}

void OutputSectData::dumpMap(llvm::raw_ostream &Outs, bool UseColor,
                             bool UseNewLine, bool WithValues,
                             bool AddIndent) const {
  if (UseColor)
    Outs.changeColor(llvm::raw_ostream::BLUE);
  Outs << getOSDKindAsStr().upper() << " ";
  Outs << "(";
  ExpressionToEvaluate.dump(Outs);
  Outs << ") ";
  ELFSection *S = getRuleContainer()->getSection();
  Outs << "\t0x";
  Outs.write_hex(S->offset());
  Outs << "\t0x";
  Outs.write_hex(S->size());
  if (UseNewLine)
    Outs << "\n";
  if (UseColor)
    Outs.resetColor();
}

void OutputSectData::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  doIndent(Outs);
  Outs << getOSDKindAsStr().upper() << " ";
  Outs << "(";
  ExpressionToEvaluate.dump(Outs);
  Outs << ")";
  Outs << "\n";
}

LinkerVersionOutputData *LinkerVersionOutputData::create(uint32_t ID,
                                                         OutputSectDesc &Out) {
  InputSectDesc::Spec Spec;
  Spec.initialize();
  return eld::make<LinkerVersionOutputData>(ID, InputSectDesc::Policy::Keep,
                                            Spec, Out);
}

LinkerVersionOutputData::LinkerVersionOutputData(uint32_t ID,
                                                 InputSectDesc::Policy Policy,
                                                 const InputSectDesc::Spec Spec,
                                                 OutputSectDesc &OutSectDesc)
    : InputSectDesc(ScriptCommand::Kind::OUTPUT_SECT_DATA, ID, Policy, Spec,
                    OutSectDesc) {}

eld::Expected<void> LinkerVersionOutputData::activate(Module &Module) {
  std::pair<SectionMap::mapping, bool> Mapping =
      Module.getScript().sectionMap().insert(*this, OutputSectionDescription);
  ASSERT(Mapping.second,
         "New rule must be created for each LINKER_VERSION directive!");
  ThisRuleContainer = Mapping.first.second;

  ELFSection *S = createSection(Module);
  RuleContainer *R = ThisRuleContainer;
  S->setOutputSection(R->getSection()->getOutputSection());
  R->incMatchCount();
  S->setMatchedLinkerScriptRule(R);
  return eld::Expected<void>();
}

ELFSection *LinkerVersionOutputData::createSection(Module &Module) {
  ASSERT(ThisRuleContainer, "Rule container must be set before emitting data!");
  RuleContainer *R = ThisRuleContainer;

  llvm::StringRef OutputSectName = R->getSection()->getOutputSection()->name();
  std::string Name = "__LinkerVersionData." + OutputSectName.str();
  ELFSection *S = Module.createInternalSection(
      Module::InternalInputType::OutputSectData, LDFileFormat::OutputSectData,
      Name, OutputSectData::DefaultSectionType,
      OutputSectData::DefaultSectionFlags,
      /*alignment=*/1);
  Fragment *F = make<StringFragment>(buildLinkerVersionString(), S);
  LayoutInfo *layoutInfo = Module.getLayoutInfo();
  if (layoutInfo)
    layoutInfo->recordFragment(
        Module.getInternalInput(Module::InternalInputType::OutputSectData), S,
        F);
  S->addFragmentAndUpdateSize(F);
  return S;
}
