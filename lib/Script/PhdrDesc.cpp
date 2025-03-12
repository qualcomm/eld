//===- PhdrDesc.cpp--------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/PhdrDesc.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Support/Utils.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// PhdrDesc
//===----------------------------------------------------------------------===//
PhdrDesc::PhdrDesc(const PhdrSpec &pSpec)
    : ScriptCommand(ScriptCommand::PHDR_DESC), m_Spec(pSpec) {}

PhdrDesc::~PhdrDesc() {}

void PhdrDesc::dump(llvm::raw_ostream &outs) const {
  outs << m_Spec.name();
  outs << " " << eld::ELFSegment::TypeToELFTypeStr(m_Spec.type()).str();
  if (m_Spec.hasFileHdr())
    outs << " "
         << "FILEHDR";
  if (m_Spec.hasPhdr())
    outs << " "
         << "PHDRS";
  if (m_Spec.atAddress()) {
    outs << " "
         << "AT(";
    m_Spec.atAddress()->dump(outs);
    outs << ")";
  }
  if (m_Spec.flags()) {
    outs << " "
         << "FLAGS(";
    m_Spec.flags()->dump(outs);
    outs << ")";
  }
  outs << ";";
  outs << "\n";
}

eld::Expected<void> PhdrDesc::activate(Module &pModule) {
  if (m_Spec.flags())
    m_Spec.flags()->setContext(getContext());
  if (m_Spec.atAddress())
    m_Spec.atAddress()->setContext(getContext());
  pModule.getScript().insertPhdrSpec(this->spec());
  if (m_Spec.type() == llvm::ELF::PT_PHDR)
    pModule.getScript().sethasPTPHDR();
  return eld::Expected<void>();
}
