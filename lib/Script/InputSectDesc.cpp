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
InputSectDesc::InputSectDesc(uint32_t ID, Policy pPolicy, const Spec &pSpec,
                             OutputSectDesc &pOutputDesc)
    : InputSectDesc(ScriptCommand::Kind::INPUT_SECT_DESC, ID, pPolicy, pSpec,
                    pOutputDesc) {}

InputSectDesc::InputSectDesc(ScriptCommand::Kind kind, uint32_t ID,
                             Policy policy, const Spec &spec,
                             OutputSectDesc &outputDesc)
    : ScriptCommand(kind), m_RuleContainer(nullptr), m_Policy(policy),
      m_OutputSectDesc(outputDesc), ID(ID) {
  m_Spec.initialize(spec);
}

InputSectDesc::~InputSectDesc() {}

void InputSectDesc::dump(llvm::raw_ostream &outs) const { dumpMap(outs); }

void InputSectDesc::dumpSpec(llvm::raw_ostream &outs) const {
  if (m_Spec.hasFile()) {
    if (m_Spec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
      outs << "SORT (";
    if (!m_Spec.file().name().empty())
      outs << m_Spec.file().getDecoratedName();
    if (m_Spec.isArchive())
      outs << ":";
    if (m_Spec.isArchive() && m_Spec.hasArchiveMember())
      outs << m_Spec.archiveMember().getDecoratedName();
  }

  if (!m_Spec.hasSections()) {
    if (m_Spec.hasFile()) {
      if (m_Spec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
        outs << ")";
    }
    return;
  }

  outs << "(";

  if (m_Spec.hasSections()) {
    bool isFirst = true;
    for (const auto &elem : m_Spec.sections()) {
      assert((elem)->kind() == StrToken::Wildcard);
      WildcardPattern *wildcard = llvm::cast<WildcardPattern>(elem);

      switch (wildcard->sortPolicy()) {
      case WildcardPattern::SORT_BY_NAME:
        outs << " SORT_BY_NAME(";
        break;
      case WildcardPattern::SORT_BY_INIT_PRIORITY:
        outs << " SORT_BY_INIT_PRIORITY(";
        break;
      case WildcardPattern::SORT_BY_ALIGNMENT:
        outs << " SORT_BY_ALIGNMENT(";
        break;
      case WildcardPattern::SORT_BY_NAME_ALIGNMENT:
        outs << " SORT_BY_NAME_ALIGNMENT(";
        break;
      case WildcardPattern::SORT_BY_ALIGNMENT_NAME:
        outs << " SORT_BY_ALIGNMENT_NAME(";
        break;
      case WildcardPattern::EXCLUDE:
        if (wildcard->excludeFiles()) {
          const ExcludeFiles *list = wildcard->excludeFiles();
          outs << " EXCLUDE_FILE (";
          for (const auto &list_it : *list) {
            if ((list_it)->isArchive())
              outs << (list_it)->archive()->getDecoratedName() << ":";
            if (!((list_it)->isFileInArchive()))
              outs << " ";
            if ((list_it)->isFile())
              outs << (list_it)->file()->getDecoratedName() << " ";
          }
        }
        outs << ")";
        LLVM_FALLTHROUGH;
      default:
        break;
      }

      if (isFirst) {
        outs << wildcard->getDecoratedName();
        isFirst = false;
      } else {
        outs << " ";
        outs << wildcard->getDecoratedName();
      }
      if ((wildcard->sortPolicy() != WildcardPattern::SORT_NONE) &&
          (wildcard->sortPolicy() != WildcardPattern::EXCLUDE))
        outs << ")";
    }
  }
  outs << ")";
  if (m_Spec.hasFile()) {
    if (m_Spec.file().sortPolicy() == WildcardPattern::SORT_BY_NAME)
      outs << ")";
  }
}

void InputSectDesc::dumpMap(llvm::raw_ostream &outs, bool useColor,
                            bool useNewLine, bool withValues,
                            bool addIndent) const {
  if (useColor)
    outs.changeColor(llvm::raw_ostream::BLUE);
  // FIXME: Remove this code duplication.
  if (m_Spec.hasExcludeFiles()) {
    const ExcludeFiles *EF = m_Spec.getExcludeFiles();
    outs << "EXCLUDE_FILE (";
    for (const auto &it : *EF) {
      if ((it)->isArchive())
        outs << (it)->archive()->getDecoratedName() << ":";
      if (!((it)->isFileInArchive()))
        outs << " ";
      if ((it)->isFile())
        outs << (it)->file()->getDecoratedName() << " ";
    }
    outs << ") ";
  }

  if (m_Policy == Fixed)
    outs << "DONTMOVE (";

  if (m_Policy == Keep)
    outs << "KEEP (";

  if (m_Policy == KeepFixed)
    outs << "KEEP_DONTMOVE (";

  dumpSpec(outs);

  if (m_Policy == Keep || m_Policy == KeepFixed || m_Policy == Fixed)
    outs << ")";

  outs << " #Rule " << ID;

  if (hasInputFileInContext())
    outs << ", " << getContext();

  if (isSpecial())
    outs << " (Implicit rule inserted by Linker)";

  if (useNewLine)
    outs << "\n";

  if (useColor)
    outs.resetColor();
}

void InputSectDesc::dumpOnlyThis(llvm::raw_ostream &outs) const {
  doIndent(outs);

  // FIXME: Remove this code duplication.
  if (m_Spec.hasExcludeFiles()) {
    const ExcludeFiles *EF = m_Spec.getExcludeFiles();
    outs << "EXCLUDE_FILE (";
    for (const auto &it : *EF) {
      if ((it)->isArchive())
        outs << (it)->archive()->getDecoratedName() << ":";
      if (!((it)->isFileInArchive()))
        outs << " ";
      if ((it)->isFile())
        outs << (it)->file()->getDecoratedName() << " ";
    }
    outs << ") ";
  }

  if (m_Policy == Fixed)
    outs << "DONTMOVE(";

  if (m_Policy == Keep)
    outs << "KEEP(";

  if (m_Policy == KeepFixed)
    outs << "KEEP_DONTMOVE(";

  dumpSpec(outs);

  if (m_Policy == Keep || m_Policy == KeepFixed || m_Policy == Fixed)
    outs << ")";

  outs << "\n";
}

eld::Expected<void> InputSectDesc::activate(Module &pModule) {
  std::pair<SectionMap::mapping, bool> Mapping =
      pModule.getScript().sectionMap().insert(*this, m_OutputSectDesc);
  this->m_RuleContainer = Mapping.first.second;
  llvm::raw_string_ostream RuleSS(m_RuleText);
  dumpMap(RuleSS, false, false, false, false);
  auto OutSectionEntry =
      this->m_RuleContainer->getSection()->getOutputSection();
  m_Hash = llvm::hash_combine(OutSectionEntry->name(), m_RuleText,
                              OutSectionEntry->getSection()->getIndex());
  pModule.addIntoRuleContainerMap(m_Hash, this->m_RuleContainer);
  return eld::Expected<void>();
}
