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
RegionAlias::RegionAlias(const StrToken *alias, const StrToken *region)
    : ScriptCommand(ScriptCommand::REGION_ALIAS), m_Alias(alias),
      m_Region(region) {}

RegionAlias::~RegionAlias() {}

void RegionAlias::dump(llvm::raw_ostream &outs) const {
  outs << "REGION_ALIAS";
  outs << "(";
  outs << "\"" << m_Alias->name() << "\"";
  outs << ",";
  outs << "\"" << m_Region->name() << "\"";
  outs << ")";
  outs << "\n";
}

eld::Expected<void> RegionAlias::activate(Module &pModule) {
  LinkerScript &script = pModule.getLinkerScript();
  std::string RegionAliasName = m_Alias->name();
  std::string RegionName = m_Region->name();
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(
      script.insertRegionAlias(RegionAliasName, getContext()));
  auto Region = script.getMemoryRegionForRegionAlias(RegionName, getContext());
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(Region);
  auto memRegion = script.getMemoryRegion(RegionAliasName, getContext());
  if (memRegion) {
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(diag::error_alias_already_is_memory_region,
                                {RegionAliasName, getContext()}));
  }
  script.addMemoryRegion(RegionAliasName, Region.value());
  return eld::Expected<void>();
}
