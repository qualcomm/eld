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
MemoryDesc::MemoryDesc(const MemorySpec &pSpec)
    : ScriptCommand(ScriptCommand::MEMORY_DESC), m_Spec(pSpec) {}

MemoryDesc::~MemoryDesc() {}

void MemoryDesc::dump(llvm::raw_ostream &outs) const {
  outs << m_Spec.getMemoryDescriptor();
  outs << " " << m_Spec.getMemoryAttributes();
  outs << " ORIGIN = ";
  m_Spec.getOrigin()->dump(outs);
  outs << " , ";
  outs << " LENGTH = ";
  m_Spec.getLength()->dump(outs);
  outs << "\n";
}

eld::Expected<void> MemoryDesc::activate(Module &pModule) {
  if (m_Spec.getMemoryDescriptor().empty())
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(diag::error_memory_region_empty, {}));
  std::string MemoryDescName = m_Spec.getMemoryDescriptor();
  LinkerScript &script = pModule.getLinkerScript();
  if (script.insertMemoryDescriptor(MemoryDescName))
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_duplicate_memory_region, {MemoryDescName}));
  ScriptMemoryRegion *Region = eld::make<ScriptMemoryRegion>(this);
  eld::Expected<void> E = Region->parseMemoryAttributes();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(E);
  script.addMemoryRegion(MemoryDescName, Region);
  if (m_Spec.getOrigin())
    m_Spec.getOrigin()->setContext(getContext());
  if (m_Spec.getLength())
    m_Spec.getLength()->setContext(getContext());

  return eld::Expected<void>();
}
