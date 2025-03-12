//===- OutputSectDesc.cpp--------------------------------------------------===//
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
#include "eld/Script/OutputSectDesc.h"
#include "eld/Core/Module.h"
#include "eld/Script/Expression.h"
#include "eld/Script/InputSectDesc.h"
#include "eld/Script/StrToken.h"
#include "eld/Script/StringList.h"
#include "eld/Support/StringUtils.h"
#include "llvm/Support/Casting.h"

using namespace eld;

//===----------------------------------------------------------------------===//
// OutputSectDesc
//===----------------------------------------------------------------------===//
OutputSectDesc::OutputSectDesc(const std::string &pName)
    : ScriptCommand(ScriptCommand::OUTPUT_SECT_DESC), m_Name(pName),
      m_Prolog() {
  m_Prolog.m_pVMA = nullptr;
  m_Prolog.m_Type = OutputSectDesc::DEFAULT_TYPE;
  m_Prolog.m_Flag = OutputSectDesc::DEFAULT_PERMISSIONS;
  m_Prolog.m_pLMA = nullptr;
  m_Prolog.m_pAlign = nullptr;
  m_Prolog.m_pSubAlign = nullptr;
  m_Prolog.m_Constraint = OutputSectDesc::NO_CONSTRAINT;
  m_Prolog.m_pPlugin = nullptr;
  m_Prolog.m_pPluginCmd = nullptr;

  m_Epilog.m_pRegion = nullptr;
  m_Epilog.m_pLMARegion = nullptr;
  m_Epilog.m_pPhdrs = nullptr;
  m_Epilog.m_pFillExp = nullptr;
}

OutputSectDesc::~OutputSectDesc() {}

void OutputSectDesc::dump(llvm::raw_ostream &outs) const {
  outs << m_Name << "\t";

  if (m_Prolog.hasVMA()) {
    m_Prolog.vma().dump(outs);
    outs << "\t";
  }

  switch (m_Prolog.type()) {
  case NOLOAD:
    outs << "(NOLOAD)";
    break;
  case DSECT:
    outs << "(DSECT)";
    break;
  case COPY:
    outs << "(COPY)";
    break;
  case INFO:
    outs << "(INFO)";
    break;
  case OVERLAY:
    outs << "(OVERLAY)";
    break;
  default:
    break;
  }
  outs << ":\n";

  if (m_Prolog.hasLMA()) {
    outs << "\tAT(";
    m_Prolog.lma().dump(outs);
    outs << ")\n";
  }

  if (m_Prolog.hasAlign()) {
    outs << "\tALIGN(";
    m_Prolog.align().dump(outs);
    outs << ")\n";
  }

  if (m_Prolog.hasSubAlign()) {
    outs << "\tSUBALIGN(";
    m_Prolog.subAlign().dump(outs);
    outs << ")\n";
  }

  switch (m_Prolog.constraint()) {
  case ONLY_IF_RO:
    outs << "\tONLY_IF_RO\n";
    break;
  case ONLY_IF_RW:
    outs << "\tONLY_IF_RW\n";
    break;
  default:
    break;
  }

  outs << "\t{\n";
  for (const auto &elem : *this) {
    switch ((elem)->getKind()) {
    case ScriptCommand::ASSIGNMENT:
    case ScriptCommand::INPUT_SECT_DESC:
    case ScriptCommand::OUTPUT_SECT_DATA:
      outs << "\t\t";
      (elem)->dump(outs);
      break;
    case ScriptCommand::INCLUDE:
    case ScriptCommand::ENTER_SCOPE:
    case ScriptCommand::EXIT_SCOPE:
      break;
    default:
      assert(0);
      break;
    }
  }
  outs << "\t}";

  dumpEpilogue(outs);

  outs << "\n";
}

void OutputSectDesc::dumpEpilogue(llvm::raw_ostream &outs) const {
  if (m_Epilog.hasRegion())
    outs << "\t>" << m_Epilog.getVMARegionName();
  if (m_Epilog.hasLMARegion())
    outs << "\tAT>" << m_Epilog.getLMARegionName();

  if (m_Epilog.hasPhdrs()) {
    for (auto &elem : *m_Epilog.phdrs()) {
      assert((elem)->kind() == StrToken::String);
      outs << ":" << (elem)->name() << " ";
    }
  }

  if (m_Epilog.hasFillExp()) {
    outs << "= ";
    m_Epilog.fillExp()->dump(outs);
  }
}

void OutputSectDesc::dumpOnlyThis(llvm::raw_ostream &outs) const {
  doIndent(outs);
  outs << m_Name;
  if (m_Prolog.hasVMA()) {
    outs << " ";
    m_Prolog.vma().dump(outs, false);
    outs << " ";
  }
  switch (m_Prolog.type()) {
  case NOLOAD:
    outs << "(NOLOAD)";
    break;
  case PROGBITS:
    outs << "(PROGBITS)";
    break;
  case UNINIT:
    outs << "(UNINIT)";
    break;
  default:
    break;
  }

  if (m_Prolog.m_pPluginCmd) {
    outs << " ";
    m_Prolog.m_pPluginCmd->dumpPluginInfo(outs);
  }

  outs << " :";
  if (m_Prolog.hasLMA()) {
    outs << " AT(";
    m_Prolog.lma().dump(outs);
    outs << ")";
  }

  if (m_Prolog.hasAlign()) {
    outs << " ALIGN(";
    m_Prolog.align().dump(outs);
    outs << ")";
  }

  if (m_Prolog.hasSubAlign()) {
    outs << " SUBALIGN(";
    m_Prolog.subAlign().dump(outs);
    outs << ")";
  }

  switch (m_Prolog.constraint()) {
  case ONLY_IF_RO:
    outs << " ONLY_IF_RO";
    break;
  case ONLY_IF_RW:
    outs << " ONLY_IF_RW";
    break;
  default:
    break;
  }
}

void OutputSectDesc::push_back(ScriptCommand *pCommand) {
  switch (pCommand->getKind()) {
  case ScriptCommand::ASSIGNMENT:
  case ScriptCommand::INCLUDE:
  case ScriptCommand::INPUT_SECT_DESC:
  case ScriptCommand::ENTER_SCOPE:
  case ScriptCommand::EXIT_SCOPE:
  case ScriptCommand::OUTPUT_SECT_DATA:
    m_OutputSectCmds.push_back(pCommand);
    break;
  default:
    assert(0);
    break;
  }
}

void OutputSectDesc::setProlog(const Prolog &pProlog) {
  m_Prolog.m_pVMA = pProlog.m_pVMA;
  m_Prolog.m_Type = pProlog.m_Type;
  m_Prolog.m_Flag = pProlog.m_Flag;
  m_Prolog.m_pLMA = pProlog.m_pLMA;
  m_Prolog.m_pAlign = pProlog.m_pAlign;
  m_Prolog.m_pSubAlign = pProlog.m_pSubAlign;
  m_Prolog.m_Constraint = pProlog.m_Constraint;
  m_Prolog.m_pPlugin = pProlog.m_pPlugin;
  m_Prolog.m_pPluginCmd = pProlog.m_pPluginCmd;
  if (m_Prolog.m_pVMA)
    m_Prolog.m_pVMA->setContextRecursively(getContext());
  if (m_Prolog.m_pLMA)
    m_Prolog.m_pLMA->setContext(getContext());
  if (m_Prolog.m_pAlign)
    m_Prolog.m_pAlign->setContext(getContext());
  if (m_Prolog.m_pSubAlign)
    m_Prolog.m_pSubAlign->setContext(getContext());
}

eld::Expected<void> OutputSectDesc::setEpilog(const Epilog &pEpilog) {
  m_Epilog.m_pRegion = pEpilog.m_pRegion;
  m_Epilog.m_pPhdrs = pEpilog.m_pPhdrs;
  if (m_Prolog.hasLMA() && pEpilog.hasLMARegion())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        diag::error_cannot_specify_lma_and_memory_region,
        {m_Name, getContext()}));
  m_Epilog.m_pLMARegion = pEpilog.m_pLMARegion;
  if (!m_Prolog.hasLMA() && !pEpilog.hasLMARegion())
    m_Epilog.m_pLMARegion = pEpilog.m_pRegion;
  m_Epilog.m_pFillExp = pEpilog.m_pFillExp;
  if (m_Epilog.m_pFillExp)
    m_Epilog.m_pFillExp->setContext(getContext());
  return eld::Expected<void>();
}

eld::Expected<void> OutputSectDesc::activate(Module &pModule) {
  // Assignment in an output section
  OutputSectCmds assignments;

  const LinkerScript &script = pModule.getLinkerScript();
  if (m_Epilog.m_pRegion) {
    eld::Expected<eld::ScriptMemoryRegion *> memRegion =
        script.getMemoryRegion(m_Epilog.m_pRegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(memRegion);
    m_Epilog.m_pScriptMemoryRegion = memRegion.value();
    // By default assign LMA region = VMA Region when the output
    // section does not have a LMA region specified and there is no
    // LMA override
    if (!m_Prolog.hasLMA() && !m_Epilog.hasLMARegion())
      m_Epilog.m_pScriptMemoryLMARegion = m_Epilog.m_pScriptMemoryRegion;
  }
  if (m_Epilog.m_pLMARegion) {
    eld::Expected<eld::ScriptMemoryRegion *> lmaRegion =
        script.getMemoryRegion(m_Epilog.m_pLMARegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(lmaRegion);
    m_Epilog.m_pScriptMemoryLMARegion = lmaRegion.value();
  }

  for (const_iterator it = begin(), ie = end(); it != ie; ++it) {
    switch ((*it)->getKind()) {
    case ScriptCommand::ASSIGNMENT:
      assignments.push_back(*it);
      break;
    case ScriptCommand::INPUT_SECT_DESC:
    case ScriptCommand::OUTPUT_SECT_DATA: {
      (*it)->activate(pModule);

      for (auto &assignment : assignments) {
        (assignment)->activate(pModule);
      }
      assignments.clear();
      break;
    }
    case ScriptCommand::INCLUDE:
    case ScriptCommand::ENTER_SCOPE:
    case ScriptCommand::EXIT_SCOPE:
      break;
    default:
      assert(0);
      break;
    }
  }
  return eld::Expected<void>();
}

void OutputSectDesc::initialize() {
  m_Prolog.m_pVMA = nullptr;
  m_Prolog.m_pLMA = nullptr;
  m_Prolog.m_pAlign = nullptr;
  m_Prolog.m_pSubAlign = nullptr;
  m_Prolog.m_pPlugin = nullptr;
  m_Prolog.m_pPluginCmd = nullptr;
  m_Epilog.m_pRegion = nullptr;
  m_Epilog.m_pLMARegion = nullptr;
  m_Epilog.m_pPhdrs = nullptr;
  m_Epilog.m_pFillExp = nullptr;
  m_Epilog.m_pScriptMemoryRegion = nullptr;
  m_Epilog.m_pScriptMemoryLMARegion = nullptr;
}
