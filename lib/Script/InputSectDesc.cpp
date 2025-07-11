//===- InputSectDesc.cpp---------------------------------------------------===//
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

//===- InputSectDesc.cpp --------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Script/InputSectDesc.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticPrinter.h"
#include "eld/Script/ExcludeFiles.h"
#include "eld/Script/OutputSectData.h"
#include "eld/Script/ScriptCommand.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// InputSectDesc
//===----------------------------------------------------------------------===//
InputSectDesc::InputSectDesc(uint32_t ID, Policy PPolicy, const Spec &PSpec,
                             OutputSectDesc &POutputDesc)
    : InputSectDesc(ScriptCommand::Kind::INPUT_SECT_DESC, ID, PPolicy, PSpec,
                    POutputDesc) {}

InputSectDesc::InputSectDesc(ScriptCommand::Kind Kind, uint32_t ID,
                             Policy Policy, const Spec &Spec,
                             OutputSectDesc &OutputDesc)
    : ScriptCommand(Kind), ThisRuleContainer(nullptr), InputSpecPolicy(Policy),
      OutputSectionDescription(OutputDesc), ID(ID) {
  InputSpec.initialize(Spec);
}

InputSectDesc::~InputSectDesc() {}

void InputSectDesc::dump(llvm::raw_ostream &Outs) const { dumpMap(Outs); }

void InputSectDesc::dumpSpec(llvm::raw_ostream &Outs) const {
  if (InputSpec.hasFile()) {
    if (InputSpec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
      Outs << "SORT (";
    if (!InputSpec.file().name().empty())
      Outs << InputSpec.file().getDecoratedName();
    if (InputSpec.isArchive())
      Outs << ":";
    if (InputSpec.isArchive() && InputSpec.hasArchiveMember())
      Outs << InputSpec.archiveMember().getDecoratedName();
  }

  if (!InputSpec.hasSections()) {
    if (InputSpec.hasFile()) {
      if (InputSpec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
        Outs << ")";
    }
    return;
  }

  Outs << "(";

  if (InputSpec.hasSections()) {
    bool IsFirst = true;
    for (const auto &Elem : InputSpec.sections()) {
      assert((Elem)->kind() == StrToken::Wildcard);
      WildcardPattern *Wildcard = llvm::cast<WildcardPattern>(Elem);

      switch (Wildcard->sortPolicy()) {
      case WildcardPattern::SORT_BY_NAME:
        Outs << " SORT_BY_NAME(";
        break;
      case WildcardPattern::SORT_BY_INIT_PRIORITY:
        Outs << " SORT_BY_INIT_PRIORITY(";
        break;
      case WildcardPattern::SORT_BY_ALIGNMENT:
        Outs << " SORT_BY_ALIGNMENT(";
        break;
      case WildcardPattern::SORT_BY_NAME_ALIGNMENT:
        Outs << " SORT_BY_NAME_ALIGNMENT(";
        break;
      case WildcardPattern::SORT_BY_ALIGNMENT_NAME:
        Outs << " SORT_BY_ALIGNMENT_NAME(";
        break;
      case WildcardPattern::EXCLUDE:
        if (Wildcard->excludeFiles()) {
          const ExcludeFiles *List = Wildcard->excludeFiles();
          Outs << " EXCLUDE_FILE (";
          for (const auto &ListIt : *List) {
            if ((ListIt)->isArchive())
              Outs << (ListIt)->archive()->getDecoratedName() << ":";
            if (!((ListIt)->isFileInArchive()))
              Outs << " ";
            if ((ListIt)->isFile())
              Outs << (ListIt)->file()->getDecoratedName() << " ";
          }
        }
        Outs << ")";
        LLVM_FALLTHROUGH;
      default:
        break;
      }

      if (IsFirst) {
        Outs << Wildcard->getDecoratedName();
        IsFirst = false;
      } else {
        Outs << " ";
        Outs << Wildcard->getDecoratedName();
      }
      if ((Wildcard->sortPolicy() != WildcardPattern::SORT_NONE) &&
          (Wildcard->sortPolicy() != WildcardPattern::EXCLUDE))
        Outs << ")";
    }
  }
  Outs << ")";
  if (InputSpec.hasFile()) {
    if (InputSpec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
      Outs << ")";
  }
}

void InputSectDesc::dumpMap(llvm::raw_ostream &Outs, bool UseColor,
                            bool UseNewLine, bool WithValues,
                            bool AddIndent) const {
  if (UseColor)
    Outs.changeColor(llvm::raw_ostream::BLUE);
  // FIXME: Remove this code duplication.
  if (InputSpec.hasExcludeFiles()) {
    const ExcludeFiles *EF = InputSpec.getExcludeFiles();
    Outs << "EXCLUDE_FILE (";
    for (const auto &It : *EF) {
      if ((It)->isArchive())
        Outs << (It)->archive()->getDecoratedName() << ":";
      if (!((It)->isFileInArchive()))
        Outs << " ";
      if ((It)->isFile())
        Outs << (It)->file()->getDecoratedName() << " ";
    }
    Outs << ") ";
  }

  if (InputSpecPolicy == Fixed)
    Outs << "DONTMOVE (";

  if (InputSpecPolicy == Keep)
    Outs << "KEEP (";

  if (InputSpecPolicy == KeepFixed)
    Outs << "KEEP_DONTMOVE (";

  dumpSpec(Outs);

  if (InputSpecPolicy == Keep || InputSpecPolicy == KeepFixed ||
      InputSpecPolicy == Fixed)
    Outs << ")";

  Outs << " #Rule " << ID;

  if (hasInputFileInContext())
    Outs << ", " << getContext();

  if (isSpecial())
    Outs << " (Implicit rule inserted by Linker)";

  if (UseNewLine)
    Outs << "\n";

  if (UseColor)
    Outs.resetColor();
}

void InputSectDesc::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  doIndent(Outs);

  // FIXME: Remove this code duplication.
  if (InputSpec.hasExcludeFiles()) {
    const ExcludeFiles *EF = InputSpec.getExcludeFiles();
    Outs << "EXCLUDE_FILE (";
    for (const auto &It : *EF) {
      if ((It)->isArchive())
        Outs << (It)->archive()->getDecoratedName() << ":";
      if (!((It)->isFileInArchive()))
        Outs << " ";
      if ((It)->isFile())
        Outs << (It)->file()->getDecoratedName() << " ";
    }
    Outs << ") ";
  }

  if (InputSpecPolicy == Fixed)
    Outs << "DONTMOVE(";

  if (InputSpecPolicy == Keep)
    Outs << "KEEP(";

  if (InputSpecPolicy == KeepFixed)
    Outs << "KEEP_DONTMOVE(";

  dumpSpec(Outs);

  if (InputSpecPolicy == Keep || InputSpecPolicy == KeepFixed ||
      InputSpecPolicy == Fixed)
    Outs << ")";

  Outs << "\n";
}

eld::Expected<void> InputSectDesc::activate(Module &CurModule) {
  std::pair<SectionMap::mapping, bool> Mapping =
      CurModule.getScript().sectionMap().insert(*this,
                                                OutputSectionDescription);
  this->ThisRuleContainer = Mapping.first.second;
  llvm::raw_string_ostream RuleSS(RuleText);
  dumpMap(RuleSS, false, false, false, false);
  auto *OutSectionEntry =
      this->ThisRuleContainer->getSection()->getOutputSection();
  Hash = llvm::hash_combine(OutSectionEntry->name(), RuleText,
                            OutSectionEntry->getSection()->getIndex());
  ThisRuleContainer->setRuleHash(Hash);
  return eld::Expected<void>();
}
