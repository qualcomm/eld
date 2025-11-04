//===- ExternCmd.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/ExternCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Fragment/FragmentRef.h"
#include "eld/Script/ScriptSymbol.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "eld/SymbolResolver/NamePool.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// ExternCmd
//===----------------------------------------------------------------------===//
ExternCmd::ExternCmd(StringList &PExtern)
    : ScriptCommand(ScriptCommand::EXTERN), ExternSymbolList(PExtern) {}

void ExternCmd::dump(llvm::raw_ostream &Outs) const {
  for (auto &E : ExternSymbolList)
    Outs << "EXTERN(" << E->name() << ")"
         << "\n";
}

eld::Expected<void> ExternCmd::activate(Module &CurModule) {
  InputFile *I =
      CurModule.getInternalInput(Module::InternalInputType::ExternList);
  auto *IRBuilder = CurModule.getIRBuilder();
  for (auto &E : ExternSymbolList) {
    std::string Name = E->name();
    LDSymbol *LDSym =
        IRBuilder->addSymbol<IRBuilder::SymbolDefinePolicy::Force,
                             IRBuilder::SymbolResolvePolicy::Resolve>(
            I, Name, ResolveInfo::Type::NoType, ResolveInfo::Desc::Undefined,
            ResolveInfo::Binding::Global, /*Size=*/0, /*Value=*/0,
            /*CurFragmentRef=*/FragmentRef::null(),
            ResolveInfo::Visibility::Default,
            /*postLTOPhase=*/false, /*IsBitcode=*/false, /*IsPatchable=*/false);
    CurModule.getConfig().options().getUndefSymList().emplace_back(
        eld::make<StrToken>(Name));
    ScriptSymbol *ScriptSym = llvm::dyn_cast_or_null<ScriptSymbol>(E);
    if (ScriptSym) {
      eld::Expected<void> E = ScriptSym->activate();
      if (!E)
        return E;
      ScriptSym->addResolveInfoToContainer(LDSym->resolveInfo());
      ThisSymbolContainers.push_back(ScriptSym->getSymbolContainer());
    }
  }
  return eld::Expected<void>();
}
