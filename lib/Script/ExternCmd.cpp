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
ExternCmd::ExternCmd(StringList &pExtern)
    : ScriptCommand(ScriptCommand::EXTERN), m_Extern(pExtern) {}

ExternCmd::~ExternCmd() {}

void ExternCmd::dump(llvm::raw_ostream &outs) const {
  for (auto &E : m_Extern)
    outs << "EXTERN(" << E->name() << ")"
         << "\n";
}

eld::Expected<void> ExternCmd::activate(Module &pModule) {
  InputFile *I =
      pModule.getInternalInput(Module::InternalInputType::ExternList);
  for (auto &E : m_Extern) {
    std::string name = E->name();
    Resolver::Result result;
    pModule.getNamePool().insertSymbol(
        I, name, false, eld::ResolveInfo::NoType, eld::ResolveInfo::Undefined,
        eld::ResolveInfo::Global, 0, 0, eld::ResolveInfo::Default, nullptr,
        result, false /* postLTOPhase*/, false, 0, false /* isPatchable */,
        pModule.getPrinter());
    pModule.getConfig().options().getUndefSymList().emplace_back(
        eld::make<StrToken>(name));
    // create a output LDSymbol
    LDSymbol *output_sym =
        make<LDSymbol>(result.info, pModule.getConfig().options().GCSections());
    result.info->setOutSymbol(output_sym);
    // Initialize origin.
    result.info->setResolvedOrigin(I);
    ScriptSymbol *scriptSym = llvm::dyn_cast_or_null<ScriptSymbol>(E);
    if (scriptSym) {
      eld::Expected<void> E = scriptSym->activate();
      if (!E)
        return E;
      scriptSym->addResolveInfoToContainer(result.info);
      m_SymbolContainers.push_back(scriptSym->getSymbolContainer());
    }
  }
  return eld::Expected<void>();
}
