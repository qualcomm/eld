//===- OutputSectData.cpp--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/OutputSectData.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/FillFragment.h"
#include "eld/Fragment/Fragment.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Fragment/OutputSectDataFragment.h"
#include "eld/Fragment/RegionFragment.h"
#include "eld/LayoutMap/LayoutPrinter.h"
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

OutputSectData *OutputSectData::Create(uint32_t ID, OutputSectDesc &outSectDesc,
                                       OSDKind kind, Expression &expr) {
  InputSectDesc::Spec spec;
  spec.initialize();
  InputSectDesc::Policy policy = InputSectDesc::Policy::Keep;
  OutputSectData *OSD =
      eld::make<OutputSectData>(ID, policy, spec, outSectDesc, kind, expr);
  return OSD;
}

OutputSectData::OutputSectData(uint32_t ID, InputSectDesc::Policy policy,
                               const InputSectDesc::Spec spec,
                               OutputSectDesc &outSectDesc, OSDKind kind,
                               Expression &expr)
    : InputSectDesc(ScriptCommand::Kind::OUTPUT_SECT_DATA, ID, policy, spec,
                    outSectDesc),
      m_OSDKind(kind), m_Expr(expr) {}

llvm::StringRef OutputSectData::getOSDKindAsStr() const {
#define ADD_CASE(dataKind)                                                     \
  case OSDKind::dataKind:                                                      \
    return #dataKind;
  switch (m_OSDKind) {
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
  switch (m_OSDKind) {
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
    ASSERT(0, "Invalid output section data: " + getOSDKindAsStr().str());
  }
}

eld::Expected<void> OutputSectData::activate(Module &module) {
  m_Expr.setContext(getContext());
  std::pair<SectionMap::mapping, bool> mapping =
      module.getScript().sectionMap().insert(*this, m_OutputSectDesc);
  ASSERT(mapping.second,
         "New rule must be created for each output section data!");
  m_RuleContainer = mapping.first.second;

  m_Section = createOSDSection(module);

  // FIXME: We need to perform the below steps whenever we associate
  // a rule with a section. We can perhaps simplify this process by
  // creating a utility function which performs the below steps.
  RuleContainer *R = m_RuleContainer;
  m_Section->setOutputSection(R->getSection()->getOutputSection());
  R->incMatchCount();
  m_Section->setMatchedLinkerScriptRule(R);
  return eld::Expected<void>();
}

ELFSection *OutputSectData::createOSDSection(Module &module) {
  ASSERT(m_RuleContainer, "Rule container must be set before the output data "
                          "section can be created!");
  RuleContainer *R = m_RuleContainer;

  llvm::StringRef outputSectName = R->getSection()->getOutputSection()->name();
  std::string name = "__OutputSectData." + outputSectName.str() + "." +
                     getOSDKindAsStr().str();
  ELFSection *S = module.createInternalSection(
      Module::InternalInputType::OutputSectData, LDFileFormat::OutputSectData,
      name, defaultSectionType, defaultSectionFlags, /*alignment=*/1);
  Fragment *F = make<OutputSectDataFragment>(*this);
  LayoutPrinter *printer = module.getLayoutPrinter();
  if (printer)
    printer->recordFragment(
        module.getInternalInput(Module::InternalInputType::OutputSectData), S,
        F);
  S->addFragmentAndUpdateSize(F);
  return S;
}

void OutputSectData::dump(llvm::raw_ostream &outs) const {
  return dumpMap(outs);
}

void OutputSectData::dumpMap(llvm::raw_ostream &outs, bool useColor,
                             bool useNewLine, bool withValues,
                             bool addIndent) const {
  if (useColor)
    outs.changeColor(llvm::raw_ostream::BLUE);
  outs << getOSDKindAsStr().upper() << " ";
  outs << "(";
  m_Expr.dump(outs);
  outs << ") ";
  ELFSection *S = getRuleContainer()->getSection();
  outs << "\t0x";
  outs.write_hex(S->offset());
  outs << "\t0x";
  outs.write_hex(S->size());
  if (useNewLine)
    outs << "\n";
  if (useColor)
    outs.resetColor();
}

void OutputSectData::dumpOnlyThis(llvm::raw_ostream &outs) const {
  doIndent(outs);
  outs << getOSDKindAsStr().upper() << " ";
  outs << "(";
  m_Expr.dump(outs);
  outs << ")";
  outs << "\n";
}
