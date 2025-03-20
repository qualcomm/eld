//===- ScriptSymbol.h------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef ELD_SCRIPT_SCRIPTSYMBOL_H
#define ELD_SCRIPT_SCRIPTSYMBOL_H

#include "eld/PluginAPI/Expected.h"
#include "eld/Script/WildcardPattern.h"
#include <string>

namespace eld {
class SymbolContainer;
class ResolveInfo;
/** \class ScriptSymbol
 *  \brief This class defines the interfaces to a element in EXCLUDE_FILE
 * list or DYNAMICLIST or VERSIONSCRIPT.
 */

class ScriptSymbol : public WildcardPattern {
public:
  explicit ScriptSymbol(const std::string &PString);

  eld::Expected<void> activate();

  SymbolContainer *getSymbolContainer() const { return ThisSymbolContainer; }

  bool matched(const ResolveInfo &Sym) const;

  void addResolveInfoToContainer(const ResolveInfo *Info) const;

private:
  SymbolContainer *ThisSymbolContainer = nullptr;
};

} // namespace eld

#endif
