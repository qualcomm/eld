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
      OutputSectDescProlog() {
  OutputSectDescProlog.OutputSectionVMA = nullptr;
  OutputSectDescProlog.ThisType = OutputSectDesc::DEFAULT_TYPE;
  OutputSectDescProlog.SectionFlag = OutputSectDesc::DEFAULT_PERMISSIONS;
  OutputSectDescProlog.OutputSectionLMA = nullptr;
  OutputSectDescProlog.Alignment = nullptr;
  OutputSectDescProlog.OutputSectionSubaAlign = nullptr;
  OutputSectDescProlog.SectionConstraint = OutputSectDesc::NO_CONSTRAINT;
  OutputSectDescProlog.ThisPlugin = nullptr;
  OutputSectDescProlog.PluginCmd = nullptr;

  OutputSectDescEpilog.OutputSectionMemoryRegion = nullptr;
  OutputSectDescEpilog.OutputSectionLMARegion = nullptr;
  OutputSectDescEpilog.ScriptPhdrs = nullptr;
  OutputSectDescEpilog.FillExpression = nullptr;
}

OutputSectDesc::~OutputSectDesc() {}

void OutputSectDesc::dump(llvm::raw_ostream &Outs) const {
  Outs << Name << "\t";

  if (OutputSectDescProlog.hasVMA()) {
    OutputSectDescProlog.vma().dump(Outs);
    Outs << "\t";
  }

  switch (OutputSectDescProlog.type()) {
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

  if (OutputSectDescProlog.hasLMA()) {
    Outs << "\tAT(";
    OutputSectDescProlog.lma().dump(Outs);
    Outs << ")\n";
  }

  if (OutputSectDescProlog.hasAlign()) {
    Outs << "\tALIGN(";
    OutputSectDescProlog.align().dump(Outs);
    Outs << ")\n";
  }

  if (OutputSectDescProlog.hasAlignWithInput()) {
    Outs << "\tALIGN_WITH_INPUT";
    Outs << ")\n";
  }

  if (OutputSectDescProlog.hasSubAlign()) {
    Outs << "\tSUBALIGN(";
    OutputSectDescProlog.subAlign().dump(Outs);
    Outs << ")\n";
  }

  switch (OutputSectDescProlog.constraint()) {
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
  if (OutputSectDescEpilog.hasRegion())
    Outs << "\t>" << OutputSectDescEpilog.getVMARegionName();
  if (OutputSectDescEpilog.hasLMARegion())
    Outs << "\tAT>" << OutputSectDescEpilog.getLMARegionName();

  if (OutputSectDescEpilog.hasPhdrs()) {
    for (auto &Elem : *OutputSectDescEpilog.phdrs()) {
      assert((Elem)->kind() == StrToken::String);
      Outs << ":" << (Elem)->name() << " ";
    }
  }

  if (OutputSectDescEpilog.hasFillExp()) {
    Outs << "= ";
    OutputSectDescEpilog.fillExp()->dump(Outs);
  }
}

void OutputSectDesc::dumpOnlyThis(llvm::raw_ostream &Outs) const {
  doIndent(Outs);
  Outs << Name;
  if (OutputSectDescProlog.hasVMA()) {
    Outs << " ";
    OutputSectDescProlog.vma().dump(Outs, false);
    Outs << " ";
  }
  switch (OutputSectDescProlog.type()) {
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

  if (OutputSectDescProlog.PluginCmd) {
    Outs << " ";
    OutputSectDescProlog.PluginCmd->dumpPluginInfo(Outs);
  }

  Outs << " :";
  if (OutputSectDescProlog.hasLMA()) {
    Outs << " AT(";
    OutputSectDescProlog.lma().dump(Outs);
    Outs << ")";
  }

  if (OutputSectDescProlog.hasAlign()) {
    Outs << " ALIGN(";
    OutputSectDescProlog.align().dump(Outs);
    Outs << ")";
  }

  if (OutputSectDescProlog.hasAlignWithInput()) {
    Outs << " ALIGN_WITH_INPUT";
  }

  if (OutputSectDescProlog.hasSubAlign()) {
    Outs << " SUBALIGN(";
    OutputSectDescProlog.subAlign().dump(Outs);
    Outs << ")";
  }

  switch (OutputSectDescProlog.constraint()) {
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
  OutputSectDescProlog.OutputSectionVMA = PProlog.OutputSectionVMA;
  OutputSectDescProlog.ThisType = PProlog.ThisType;
  OutputSectDescProlog.SectionFlag = PProlog.SectionFlag;
  OutputSectDescProlog.OutputSectionLMA = PProlog.OutputSectionLMA;
  OutputSectDescProlog.Alignment = PProlog.Alignment;
  OutputSectDescProlog.OutputSectionSubaAlign = PProlog.OutputSectionSubaAlign;
  OutputSectDescProlog.SectionConstraint = PProlog.SectionConstraint;
  OutputSectDescProlog.ThisPlugin = PProlog.ThisPlugin;
  OutputSectDescProlog.PluginCmd = PProlog.PluginCmd;
  OutputSectDescProlog.HasAlignWithInput = PProlog.HasAlignWithInput;
  if (OutputSectDescProlog.OutputSectionVMA)
    OutputSectDescProlog.OutputSectionVMA->setContextRecursively(getContext());
  if (OutputSectDescProlog.OutputSectionLMA)
    OutputSectDescProlog.OutputSectionLMA->setContext(getContext());
  if (OutputSectDescProlog.Alignment)
    OutputSectDescProlog.Alignment->setContext(getContext());
  if (OutputSectDescProlog.OutputSectionSubaAlign)
    OutputSectDescProlog.OutputSectionSubaAlign->setContext(getContext());
}

eld::Expected<void> OutputSectDesc::setEpilog(const Epilog &PEpilog) {
  OutputSectDescEpilog.OutputSectionMemoryRegion =
      PEpilog.OutputSectionMemoryRegion;
  OutputSectDescEpilog.ScriptPhdrs = PEpilog.ScriptPhdrs;
  if (OutputSectDescProlog.hasLMA() && !PEpilog.getLMARegionName().empty())
    return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
        Diag::error_cannot_specify_lma_and_memory_region,
        {Name, getContext()}));
  OutputSectDescEpilog.OutputSectionLMARegion = PEpilog.OutputSectionLMARegion;
  if (!PEpilog.OutputSectionLMARegion)
    OutputSectDescEpilog.OutputSectionLMARegion =
        PEpilog.OutputSectionMemoryRegion;
  OutputSectDescEpilog.FillExpression = PEpilog.FillExpression;
  if (OutputSectDescEpilog.FillExpression)
    OutputSectDescEpilog.FillExpression->setContext(getContext());
  return eld::Expected<void>();
}

eld::Expected<void> OutputSectDesc::activate(Module &CurModule) {
  // Assignment in an output section
  OutputSectCmds Assignments;
  const LinkerScript &Script = CurModule.getLinkerScript();
  if (OutputSectDescEpilog.OutputSectionMemoryRegion) {
    eld::Expected<eld::ScriptMemoryRegion *> MemRegion = Script.getMemoryRegion(
        OutputSectDescEpilog.OutputSectionMemoryRegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(MemRegion);
    OutputSectDescEpilog.ScriptVMAMemoryRegion = MemRegion.value();
    // By default assign LMA region = VMA Region when the output
    // section does not have a LMA region specified and there is no
    // LMA override
    if (!OutputSectDescProlog.hasLMA() && !OutputSectDescEpilog.hasLMARegion())
      OutputSectDescEpilog.ScriptLMAMemoryRegion =
          OutputSectDescEpilog.ScriptVMAMemoryRegion;
  }
  if (OutputSectDescEpilog.OutputSectionLMARegion) {
    eld::Expected<eld::ScriptMemoryRegion *> LmaRegion = Script.getMemoryRegion(
        OutputSectDescEpilog.OutputSectionLMARegion->name(), getContext());
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(LmaRegion);
    OutputSectDescEpilog.ScriptLMAMemoryRegion = LmaRegion.value();
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
  // Add undefined symbols to the NamePool that are referred in the output
  // section description prologue.
  NamePool &NP = CurModule.getNamePool();
  ASSERT(getInputFileInContext(),
         "OutputSectDesc must have input file in context!");
  InputFile *IF = getInputFileInContext();
  if (OutputSectDescProlog.hasVMA())
    OutputSectDescProlog.vma().addRefSymbolsAsUndefSymbolToNP(IF, NP);
  if (OutputSectDescProlog.hasLMA())
    OutputSectDescProlog.lma().addRefSymbolsAsUndefSymbolToNP(IF, NP);
  if (OutputSectDescProlog.hasAlign())
    OutputSectDescProlog.align().addRefSymbolsAsUndefSymbolToNP(IF, NP);
  if (OutputSectDescProlog.hasSubAlign())
    OutputSectDescProlog.subAlign().addRefSymbolsAsUndefSymbolToNP(IF, NP);
  return eld::Expected<void>();
}

void OutputSectDesc::initialize() {
  OutputSectDescProlog.OutputSectionVMA = nullptr;
  OutputSectDescProlog.OutputSectionLMA = nullptr;
  OutputSectDescProlog.Alignment = nullptr;
  OutputSectDescProlog.OutputSectionSubaAlign = nullptr;
  OutputSectDescProlog.ThisPlugin = nullptr;
  OutputSectDescProlog.PluginCmd = nullptr;
  OutputSectDescEpilog.OutputSectionMemoryRegion = nullptr;
  OutputSectDescEpilog.OutputSectionLMARegion = nullptr;
  OutputSectDescEpilog.ScriptPhdrs = nullptr;
  OutputSectDescEpilog.FillExpression = nullptr;
  OutputSectDescEpilog.ScriptVMAMemoryRegion = nullptr;
  OutputSectDescEpilog.ScriptLMAMemoryRegion = nullptr;
}
