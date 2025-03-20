//===- MemoryDesc.cpp------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/MemoryDesc.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Support/Utils.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// MemoryDesc
//===----------------------------------------------------------------------===//
MemoryDesc::MemoryDesc(const MemorySpec &PSpec)
    : ScriptCommand(ScriptCommand::MEMORY_DESC), InputSpec(PSpec) {}

MemoryDesc::~MemoryDesc() {}

void MemoryDesc::dump(llvm::raw_ostream &Outs) const {
  Outs << InputSpec.getMemoryDescriptor();
  Outs << " " << InputSpec.getMemoryAttributes();
  Outs << " ORIGIN = ";
  InputSpec.getOrigin()->dump(Outs);
  Outs << " , ";
  Outs << " LENGTH = ";
  InputSpec.getLength()->dump(Outs);
  Outs << "\n";
}

eld::Expected<void> MemoryDesc::activate(Module &CurModule) {
  if (InputSpec.getMemoryDescriptor().empty())
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::error_memory_region_empty, {}));
  std::string MemoryDescName = InputSpec.getMemoryDescriptor();
  LinkerScript &Script = CurModule.getLinkerScript();
  if (Script.insertMemoryDescriptor(MemoryDescName))
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_duplicate_memory_region, {MemoryDescName}));
  ScriptMemoryRegion *Region = eld::make<ScriptMemoryRegion>(this);
  eld::Expected<void> E = Region->parseMemoryAttributes();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  Script.addMemoryRegion(MemoryDescName, Region);
  if (InputSpec.getOrigin())
    InputSpec.getOrigin()->setContext(getContext());
  if (InputSpec.getLength())
    InputSpec.getLength()->setContext(getContext());

  return eld::Expected<void>();
}
