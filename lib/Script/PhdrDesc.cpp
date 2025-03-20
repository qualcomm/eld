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
PhdrDesc::PhdrDesc(const PhdrSpec &PSpec)
    : ScriptCommand(ScriptCommand::PHDR_DESC), InputSpec(PSpec) {}

PhdrDesc::~PhdrDesc() {}

void PhdrDesc::dump(llvm::raw_ostream &Outs) const {
  Outs << InputSpec.name();
  Outs << " " << eld::ELFSegment::TypeToELFTypeStr(InputSpec.type()).str();
  if (InputSpec.hasFileHdr())
    Outs << " "
         << "FILEHDR";
  if (InputSpec.hasPhdr())
    Outs << " "
         << "PHDRS";
  if (InputSpec.atAddress()) {
    Outs << " "
         << "AT(";
    InputSpec.atAddress()->dump(Outs);
    Outs << ")";
  }
  if (InputSpec.flags()) {
    Outs << " "
         << "FLAGS(";
    InputSpec.flags()->dump(Outs);
    Outs << ")";
  }
  Outs << ";";
  Outs << "\n";
}

eld::Expected<void> PhdrDesc::activate(Module &CurModule) {
  if (InputSpec.flags())
    InputSpec.flags()->setContext(getContext());
  if (InputSpec.atAddress())
    InputSpec.atAddress()->setContext(getContext());
  CurModule.getScript().insertPhdrSpec(this->spec());
  if (InputSpec.type() == llvm::ELF::PT_PHDR)
    CurModule.getScript().sethasPTPHDR();
  return eld::Expected<void>();
}
