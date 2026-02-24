//===- OverlayDesc.h------------------------------------------------------===//
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

#ifndef ELD_SCRIPT_OVERLAYDESC_H
#define ELD_SCRIPT_OVERLAYDESC_H

#include "eld/Script/OutputSectDesc.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace eld {

class Expression;
class StrToken;
class OutputSectionEntry;

class OverlayDesc final {
public:
  OverlayDesc(uint32_t ID, Expression *Start, bool HasStart, bool NoCrossRefs,
              Expression *LMA, OutputSectDesc::Epilog Epilog)
      : ID(ID), Start(Start), HasStart(HasStart), NoCrossRefs(NoCrossRefs),
        LMA(LMA), Epilog(std::move(Epilog)) {}

  uint32_t id() const { return ID; }

  bool hasStart() const { return HasStart && Start != nullptr; }
  Expression *start() const { return Start; }

  bool hasNoCrossRefs() const { return NoCrossRefs; }

  bool hasLMA() const { return LMA != nullptr; }
  Expression *lma() const { return LMA; }

  const OutputSectDesc::Epilog &epilog() const { return Epilog; }

  void addPendingMemberName(const StrToken *Name) {
    PendingMemberNames.push_back(Name);
  }

  llvm::ArrayRef<const StrToken *> pendingMemberNames() const {
    return PendingMemberNames;
  }

  void addMember(OutputSectionEntry *OSE) { Members.push_back(OSE); }

  llvm::ArrayRef<OutputSectionEntry *> members() const { return Members; }

  void dump(llvm::raw_ostream &Outs) const {
    Outs << "OVERLAY";
    if (hasStart()) {
      Outs << " ";
      Start->dump(Outs, /*WithValues=*/false);
    }
    Outs << " :";
    if (NoCrossRefs)
      Outs << " NOCROSSREFS";
    if (LMA) {
      Outs << " AT(";
      LMA->dump(Outs, /*WithValues=*/false);
      Outs << ")";
    }
    Outs << "\n{\n";
    for (const StrToken *M : PendingMemberNames)
      Outs << "\t" << (M ? M->name() : "<null>") << "\n";
    Outs << "}";

    if (Epilog.OutputSectionMemoryRegion)
      Outs << " >" << Epilog.OutputSectionMemoryRegion->name();
    if (Epilog.OutputSectionLMARegion)
      Outs << " AT>" << Epilog.OutputSectionLMARegion->name();
    if (Epilog.ScriptPhdrs && !Epilog.ScriptPhdrs->empty()) {
      for (auto &Elem : *Epilog.ScriptPhdrs) {
        assert((Elem)->kind() == StrToken::String);
        Outs << ":" << (Elem)->name() << " ";
      }
    }
    if (Epilog.FillExpression) {
      Outs << "= ";
      Epilog.FillExpression->dump(Outs, /*WithValues=*/false);
    }
    Outs << "\n";
  }

private:
  uint32_t ID = 0;
  Expression *Start = nullptr;
  bool HasStart = false;
  bool NoCrossRefs = false;
  Expression *LMA = nullptr;
  OutputSectDesc::Epilog Epilog;
  llvm::SmallVector<const StrToken *, 4> PendingMemberNames;
  llvm::SmallVector<OutputSectionEntry *, 4> Members;
};

} // namespace eld

#endif
