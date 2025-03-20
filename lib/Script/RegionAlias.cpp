//===- RegionAlias.cpp-----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/RegionAlias.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// RegionAlias
//===----------------------------------------------------------------------===//
RegionAlias::RegionAlias(const StrToken *Alias, const StrToken *Region)
    : ScriptCommand(ScriptCommand::REGION_ALIAS), MemoryAliasName(Alias),
      MemoryRegionName(Region) {}

RegionAlias::~RegionAlias() {}

void RegionAlias::dump(llvm::raw_ostream &Outs) const {
  Outs << "REGION_ALIAS";
  Outs << "(";
  Outs << "\"" << MemoryAliasName->name() << "\"";
  Outs << ",";
  Outs << "\"" << MemoryRegionName->name() << "\"";
  Outs << ")";
  Outs << "\n";
}

eld::Expected<void> RegionAlias::activate(Module &CurModule) {
  LinkerScript &Script = CurModule.getLinkerScript();
  std::string RegionAliasName = MemoryAliasName->name();
  std::string RegionName = MemoryRegionName->name();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(
      Script.insertRegionAlias(RegionAliasName, getContext()));
  auto Region = Script.getMemoryRegionForRegionAlias(RegionName, getContext());
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(Region);
  auto MemRegion = Script.getMemoryRegion(RegionAliasName, getContext());
  if (MemRegion) {
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::error_alias_already_is_memory_region,
                                {RegionAliasName, getContext()}));
  }
  Script.addMemoryRegion(RegionAliasName, Region.value());
  return eld::Expected<void>();
}
