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
OutputSectDesc::OutputSectDesc(const std::string &PName)
    : ScriptCommand(ScriptCommand::OUTPUT_SECT_DESC), Name(PName),
      OutpuSectDescProlog() {
  OutpuSectDescProlog.OutputSectionVMA = nullptr;
  OutpuSectDescProlog.ThisType = OutputSectDesc::DEFAULT_TYPE;
  OutpuSectDescProlog.SectionFlag = OutputSectDesc::DEFAULT_PERMISSIONS;
  OutpuSectDescProlog.OutputSectionLMA = nullptr;
  OutpuSectDescProlog.Alignment = nullptr;
  OutpuSectDescProlog.OutputSectionSubaAlign = nullptr;
  OutpuSectDescProlog.SectionConstraint = OutputSectDesc::NO_CONSTRAINT;
  OutpuSectDescProlog.ThisPlugin = nullptr;
  OutpuSectDescProlog.PluginCmd = nullptr;

  OutpuSectDescEpilog.OutputSectionMemoryRegion = nullptr;
  OutpuSectDescEpilog.OutputSectionLMARegion = nullptr;
  OutpuSectDescEpilog.ScriptPhdrs = nullptr;
  OutpuSectDescEpilog.FillExpression = nullptr;
}

OutputSectDesc::~OutputSectDesc() {}

void OutputSectDesc::dump(llvm::raw_ostream &Outs) const {
  Outs << Name << "\t";

  if (OutpuSectDescProlog.hasVMA()) {
    OutpuSectDescProlog.vma().dump(Outs);
    Outs << "\t";
  }

  switch (OutpuSectDescProlog.type()) {
  case NOLOAD:
    Outs << "(NOLOAD)";
    break;
  case DSECT:
    Outs << "(DSECT)";
    break;
  case COPY:
    Outs << "(COPY)";
    break;
  case INFO:
    Outs << "(INFO)";
    break;
  case OVERLAY:
    Outs << "(OVERLAY)";
    break;
  default:
    break;
  }
  Outs << ":\n";

  if (OutpuSectDescProlog.hasLMA()) {
    Outs << "\tAT(";
    OutpuSectDescProlog.lma().dump(Outs);
    Outs << ")\n";
  }

  if (OutpuSectDescProlog.hasAlign()) {
    Outs << "\tALIGN(";
    OutpuSectDescProlog.align().dump(Outs);
    Outs << ")\n";
  }

  if (OutpuSectDescProlog.hasAlignWithInput()) {
    Outs << "\tALIGN_WITH_INPUT";
    Outs << ")\n";
  }

  if (OutpuSectDescProlog.hasSubAlign()) {
    Outs << "\tSUBALIGN(";
    OutpuSectDescProlog.subAlign().dump(Outs);
    Outs << ")\n";
  }

  switch (OutpuSectDescProlog.constraint()) {
  case ONLY_IF_RO:
    Outs << "\tONLY_IF_RO\n";
    break;
  case ONLY_IF_RW:
    Outs << "\tONLY_IF_RW\n";
    break;
  default:
    break;
  }

  Outs << "\t{\n";
  for (const auto &Elem : *this) {
    switch ((Elem)->getKind()) {
    case ScriptCommand::ASSIGNMENT:
    case ScriptCommand::INPUT_SECT_DESC:
    case ScriptCommand::OUTPUT_SECT_DATA:
      Outs << "\t\t";
      (Elem)->dump(Outs);
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
  Outs << "\t}";

  dumpEpilogue(Outs);

  Outs << "\n";
}

void OutputSectDesc::dumpEpilogue(llvm::raw_ostream &Outs) const {
  if (OutpuSectDescEpilog.hasRegion())
    Outs << "\t>" << OutpuSectDescEpilog.getVMARegionName();
  if (OutpuSectDescEpilog.hasLMARegion())
    Outs << "\tAT>" << OutpuSectDescEpilog.getLMARegionName();

  if (OutpuSectDescEpilog.hasPhdrs()) {
    for (auto &Elem : *OutpuSectDescEpilog.phdrs()) {
      assert((Elem)->kind() == StrToken::String);
      Outs << ":" << (Elem)->name() << " ";
    }
  }

  if (OutpuSectDescEpilog.hasFillExp()) {
    Outs << "= ";
    OutpuSectDescEpilog.fillExp()->dump(Outs);
  }
}

void OutputSectDesc::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  doIndent(Outs);
  Outs << Name;
  if (OutpuSectDescProlog.hasVMA()) {
    Outs << " ";
    OutpuSectDescProlog.vma().dump(Outs, false);
    Outs << " ";
  }
  switch (OutpuSectDescProlog.type()) {
  case NOLOAD:
    Outs << "(NOLOAD)";
    break;
  case PROGBITS:
    Outs << "(PROGBITS)";
    break;
  case UNINIT:
    Outs << "(UNINIT)";
    break;
  default:
    break;
  }

  if (OutpuSectDescProlog.PluginCmd) {
    Outs << " ";
    OutpuSectDescProlog.PluginCmd->dumpPluginInfo(Outs);
  }

  Outs << " :";
  if (OutpuSectDescProlog.hasLMA()) {
    Outs << " AT(";
    OutpuSectDescProlog.lma().dump(Outs);
    Outs << ")";
  }

  if (OutpuSectDescProlog.hasAlign()) {
    Outs << " ALIGN(";
    OutpuSectDescProlog.align().dump(Outs);
    Outs << ")";
  }

  if (OutpuSectDescProlog.hasAlignWithInput()) {
    Outs << " ALIGN_WITH_INPUT";
  }

  if (OutpuSectDescProlog.hasSubAlign()) {
    Outs << " SUBALIGN(";
    OutpuSectDescProlog.subAlign().dump(Outs);
    Outs << ")";
  }

  switch (OutpuSectDescProlog.constraint()) {
  case ONLY_IF_RO:
    Outs << " ONLY_IF_RO";
    break;
  case ONLY_IF_RW:
    Outs << " ONLY_IF_RW";
    break;
  default:
    break;
  }
}

void OutputSectDesc::pushBack(ScriptCommand *PCommand) {
  switch (PCommand->getKind()) {
  case ScriptCommand::ASSIGNMENT:
  case ScriptCommand::INCLUDE:
  case ScriptCommand::INPUT_SECT_DESC:
  case ScriptCommand::ENTER_SCOPE:
  case ScriptCommand::EXIT_SCOPE:
  case ScriptCommand::OUTPUT_SECT_DATA:
    OutputSectionCommands.push_back(PCommand);
    break;
  default:
    assert(0);
    break;
  }
}

void OutputSectDesc::setProlog(const Prolog &PProlog) {
  OutpuSectDescProlog.OutputSectionVMA = PProlog.OutputSectionVMA;
  OutpuSectDescProlog.ThisType = PProlog.ThisType;
  OutpuSectDescProlog.SectionFlag = PProlog.SectionFlag;
  OutpuSectDescProlog.OutputSectionLMA = PProlog.OutputSectionLMA;
  OutpuSectDescProlog.Alignment = PProlog.Alignment;
  OutpuSectDescProlog.OutputSectionSubaAlign = PProlog.OutputSectionSubaAlign;
  OutpuSectDescProlog.SectionConstraint = PProlog.SectionConstraint;
  OutpuSectDescProlog.ThisPlugin = PProlog.ThisPlugin;
  OutpuSectDescProlog.PluginCmd = PProlog.PluginCmd;
  OutpuSectDescProlog.HasAlignWithInput = PProlog.HasAlignWithInput;
  if (OutpuSectDescProlog.OutputSectionVMA)
    OutpuSectDescProlog.OutputSectionVMA->setContextRecursively(getContext());
  if (OutpuSectDescProlog.OutputSectionLMA)
    OutpuSectDescProlog.OutputSectionLMA->setContext(getContext());
  if (OutpuSectDescProlog.Alignment)
    OutpuSectDescProlog.Alignment->setContext(getContext());
  if (OutpuSectDescProlog.OutputSectionSubaAlign)
    OutpuSectDescProlog.OutputSectionSubaAlign->setContext(getContext());
}

eld::Expected<void> OutputSectDesc::setEpilog(const Epilog &PEpilog) {
  OutpuSectDescEpilog.OutputSectionMemoryRegion =
      PEpilog.OutputSectionMemoryRegion;
  OutpuSectDescEpilog.ScriptPhdrs = PEpilog.ScriptPhdrs;
  if (OutpuSectDescProlog.hasLMA() && PEpilog.hasLMARegion())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_specify_lma_and_memory_region,
        {Name, getContext()}));
  OutpuSectDescEpilog.OutputSectionLMARegion = PEpilog.OutputSectionLMARegion;
  if (!OutpuSectDescProlog.hasLMA() && !PEpilog.hasLMARegion())
    OutpuSectDescEpilog.OutputSectionLMARegion =
        PEpilog.OutputSectionMemoryRegion;
  OutpuSectDescEpilog.FillExpression = PEpilog.FillExpression;
  if (OutpuSectDescEpilog.FillExpression)
    OutpuSectDescEpilog.FillExpression->setContext(getContext());
  return eld::Expected<void>();
}

eld::Expected<void> OutputSectDesc::activate(Module &CurModule) {
  // Assignment in an output section
  OutputSectCmds Assignments;

  const LinkerScript &Script = CurModule.getLinkerScript();
  if (OutpuSectDescEpilog.OutputSectionMemoryRegion) {
    eld::Expected<eld::ScriptMemoryRegion *> MemRegion = Script.getMemoryRegion(
        OutpuSectDescEpilog.OutputSectionMemoryRegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(MemRegion);
    OutpuSectDescEpilog.ScriptVMAMemoryRegion = MemRegion.value();
    // By default assign LMA region = VMA Region when the output
    // section does not have a LMA region specified and there is no
    // LMA override
    if (!OutpuSectDescProlog.hasLMA() && !OutpuSectDescEpilog.hasLMARegion())
      OutpuSectDescEpilog.ScriptLMAMemoryRegion =
          OutpuSectDescEpilog.ScriptVMAMemoryRegion;
  }
  if (OutpuSectDescEpilog.OutputSectionLMARegion) {
    eld::Expected<eld::ScriptMemoryRegion *> LmaRegion = Script.getMemoryRegion(
        OutpuSectDescEpilog.OutputSectionLMARegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(LmaRegion);
    OutpuSectDescEpilog.ScriptLMAMemoryRegion = LmaRegion.value();
  }

  for (const_iterator It = begin(), Ie = end(); It != Ie; ++It) {
    switch ((*It)->getKind()) {
    case ScriptCommand::ASSIGNMENT:
      Assignments.push_back(*It);
      break;
    case ScriptCommand::INPUT_SECT_DESC:
    case ScriptCommand::OUTPUT_SECT_DATA: {
      (*It)->activate(CurModule);

      for (auto &Assignment : Assignments) {
        (Assignment)->activate(CurModule);
      }
      Assignments.clear();
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
  OutpuSectDescProlog.OutputSectionVMA = nullptr;
  OutpuSectDescProlog.OutputSectionLMA = nullptr;
  OutpuSectDescProlog.Alignment = nullptr;
  OutpuSectDescProlog.OutputSectionSubaAlign = nullptr;
  OutpuSectDescProlog.ThisPlugin = nullptr;
  OutpuSectDescProlog.PluginCmd = nullptr;
  OutpuSectDescEpilog.OutputSectionMemoryRegion = nullptr;
  OutpuSectDescEpilog.OutputSectionLMARegion = nullptr;
  OutpuSectDescEpilog.ScriptPhdrs = nullptr;
  OutpuSectDescEpilog.FillExpression = nullptr;
  OutpuSectDescEpilog.ScriptVMAMemoryRegion = nullptr;
  OutpuSectDescEpilog.ScriptLMAMemoryRegion = nullptr;
}
