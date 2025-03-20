//===- ExternCmd.cpp-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Script/ExternCmd.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
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

ExternCmd::~ExternCmd() {}

void ExternCmd::dump(llvm::raw_ostream &Outs) const {
  for (auto &E : ExternSymbolList)
    Outs << "EXTERN(" << E->name() << ")"
         << "\n";
}

eld::Expected<void> ExternCmd::activate(Module &CurModule) {
  InputFile *I =
      CurModule.getInternalInput(Module::InternalInputType::ExternList);
  for (auto &E : ExternSymbolList) {
    std::string Name = E->name();
    Resolver::Result Result;
    CurModule.getNamePool().insertSymbol(
        I, Name, false, eld::ResolveInfo::NoType, eld::ResolveInfo::Undefined,
        eld::ResolveInfo::Global, 0, 0, eld::ResolveInfo::Default, nullptr,
        Result, false /* postLTOPhase*/, false, 0, false /* isPatchable */,
        CurModule.getPrinter());
    CurModule.getConfig().options().getUndefSymList().emplace_back(
        eld::make<StrToken>(Name));
    // create a output LDSymbol
    LDSymbol *OutputSym = make<LDSymbol>(
        Result.Info, CurModule.getConfig().options().gcSections());
    Result.Info->setOutSymbol(OutputSym);
    // Initialize origin.
    Result.Info->setResolvedOrigin(I);
    ScriptSymbol *ScriptSym = llvm::dyn_cast_or_null<ScriptSymbol>(E);
    if (ScriptSym) {
      eld::Expected<void> E = ScriptSym->activate();
      if (!E)
        return E;
      ScriptSym->addResolveInfoToContainer(Result.Info);
      ThisSymbolContainers.push_back(ScriptSym->getSymbolContainer());
    }
  }
  return eld::Expected<void>();
}
