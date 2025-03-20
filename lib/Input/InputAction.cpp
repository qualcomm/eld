//===- InputAction.cpp-----------------------------------------------------===//
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

#include "eld/Input/InputAction.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputBuilder.h"
#include "eld/Input/InputTree.h"
#include "eld/Input/SearchDirs.h"
#include "eld/Support/FileSystem.h"
#include "eld/Support/MsgHandling.h"
#include <fstream>

using namespace eld;

//===----------------------------------------------------------------------===//
// Base Positional Option
//===----------------------------------------------------------------------===//
InputAction::InputAction(InputAction::InputActionKind K,
                         DiagnosticPrinter *Printer)
    : K(K) {}

InputAction::~InputAction() {}

//===----------------------------------------------------------------------===//
// InputFileAction
//===----------------------------------------------------------------------===//
InputFileAction::InputFileAction(std::string Name, DiagnosticPrinter *Printer)
    : InputAction(InputAction::InputFile, Printer), Name(Name), I(nullptr) {}

InputFileAction::InputFileAction(std::string Name,
                                 InputAction::InputActionKind K,
                                 DiagnosticPrinter *Printer)
    : InputAction(K, Printer), Name(Name), I(nullptr) {}

bool InputFileAction::activate(InputBuilder &PBuilder) {
  std::ifstream Is;
  Is.open(Name.c_str(), std::ifstream::in);
  if (!Is.good()) {
    PBuilder.getDiagEngine()->raise(Diag::fatal_cannot_read_input) << Name;
    return false;
  }
  Is.close();
  I = PBuilder.createInputNode(Name);
  return true;
}

//===----------------------------------------------------------------------===//
// NamespecAction
//===----------------------------------------------------------------------===//
NamespecAction::NamespecAction(const std::string &PNamespec,
                               DiagnosticPrinter *Printer)
    : InputAction(InputAction::Namespec, Printer), MNamespec(PNamespec) {}

bool NamespecAction::activate(InputBuilder &PBuilder) {
  I = PBuilder.createDeferredInput(MNamespec, Input::Namespec);
  return true;
}

//===----------------------------------------------------------------------===//
// StartGroupAction
//===----------------------------------------------------------------------===//
StartGroupAction::StartGroupAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::StartGroup, Printer) {}

bool StartGroupAction::activate(InputBuilder &PBuilder) {
  PBuilder.enterGroup();
  return true;
}

//===----------------------------------------------------------------------===//
// EndGroupAction
//===----------------------------------------------------------------------===//
EndGroupAction::EndGroupAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::EndGroup, Printer) {}

bool EndGroupAction::activate(InputBuilder &PBuilder) {
  PBuilder.exitGroup();
  return true;
}

//===----------------------------------------------------------------------===//
// WholeArchiveAction
//===----------------------------------------------------------------------===//
WholeArchiveAction::WholeArchiveAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::WholeArchive, Printer) {}

bool WholeArchiveAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().setWholeArchive();
  return true;
}

//===----------------------------------------------------------------------===//
// NoWholeArchiveAction
//===----------------------------------------------------------------------===//
NoWholeArchiveAction::NoWholeArchiveAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoWholeArchive, Printer) {}

bool NoWholeArchiveAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().unsetWholeArchive();
  return true;
}

//===----------------------------------------------------------------------===//
// AsNeededAction
//===----------------------------------------------------------------------===//
AsNeededAction::AsNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::AsNeeded, Printer) {}

bool AsNeededAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().setAsNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// NoAsNeededAction
//===----------------------------------------------------------------------===//
NoAsNeededAction::NoAsNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoAsNeeded, Printer) {}

bool NoAsNeededAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().unsetAsNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// AddNeededAction
//===----------------------------------------------------------------------===//
AddNeededAction::AddNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::AddNeeded, Printer) {}

bool AddNeededAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().setAddNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// NoAddNeededAction
//===----------------------------------------------------------------------===//
NoAddNeededAction::NoAddNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoAddNeeded, Printer) {}

bool NoAddNeededAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().unsetAddNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// BDynamicAction
//===----------------------------------------------------------------------===//
BDynamicAction::BDynamicAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::BDynamic, Printer) {}

bool BDynamicAction::activate(InputBuilder &PBuilder) {
  // For partial links prefer archive libraries over shared objects
  if (PBuilder.getLinkerConfig().isLinkPartial())
    PBuilder.getAttributes().setStatic();
  else
    PBuilder.getAttributes().setDynamic();
  return true;
}

//===----------------------------------------------------------------------===//
// BStaticAction
//===----------------------------------------------------------------------===//
BStaticAction::BStaticAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::BStatic, Printer) {}

bool BStaticAction::activate(InputBuilder &PBuilder) {
  PBuilder.getAttributes().setStatic();
  return true;
}

//===----------------------------------------------------------------------===//
// DefSymAction
//===----------------------------------------------------------------------===//
DefSymAction::DefSymAction(std::string PAssignment, DiagnosticPrinter *Printer)
    : InputAction(InputAction::DefSym, Printer), MAssignment(PAssignment) {}

// FIXME: add an assignment of type DefSym. This is so strange to create a
// Defsym Expression and create an input for it.
bool DefSymAction::activate(InputBuilder &PBuilder) {
  std::string FileName =
      (llvm::Twine("Expression(Defsym)") + llvm::Twine(MAssignment)).str();
  Input *Input = PBuilder.createInputNode(FileName, true /*isSpecial*/);
  Input->setInputType(Input::Script);
  Input->setResolvedPath(FileName);
  MAssignment.append(";");
  PBuilder.setMemory(*Input, &MAssignment[0], MAssignment.size());
  return true;
}

InputFormatAction::InputFormatAction(const std::string &InputFormat,
                                     DiagnosticPrinter *Printer)
    : InputAction(InputAction::InputActionKind::InputFormat, Printer),
      InputFormat(InputFormat) {}

bool InputFormatAction::activate(InputBuilder &Builder) {
  if (InputFormat == "binary")
    Builder.getAttributes().setIsBinary(true);
  else if (InputFormat == "default")
    Builder.getAttributes().setIsBinary(false);
  else {
    Builder.getDiagEngine()->raise(Diag::error_invalid_input_format)
        << InputFormat;
    return false;
  }
  return true;
}
