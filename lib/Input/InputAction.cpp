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
    : InputAction(InputAction::InputFile, Printer), m_Name(Name), I(nullptr) {}

InputFileAction::InputFileAction(std::string Name,
                                 InputAction::InputActionKind K,
                                 DiagnosticPrinter *Printer)
    : InputAction(K, Printer), m_Name(Name), I(nullptr) {}

bool InputFileAction::activate(InputBuilder &pBuilder) {
  std::ifstream is;
  is.open(m_Name.c_str(), std::ifstream::in);
  if (!is.good()) {
    pBuilder.getDiagEngine()->raise(diag::fatal_cannot_read_input) << m_Name;
    return false;
  }
  is.close();
  I = pBuilder.createInputNode(m_Name);
  return true;
}

//===----------------------------------------------------------------------===//
// NamespecAction
//===----------------------------------------------------------------------===//
NamespecAction::NamespecAction(const std::string &pNamespec,
                               DiagnosticPrinter *Printer)
    : InputAction(InputAction::Namespec, Printer), m_Namespec(pNamespec) {}

bool NamespecAction::activate(InputBuilder &pBuilder) {
  I = pBuilder.createDeferredInput(m_Namespec, Input::Namespec);
  return true;
}

//===----------------------------------------------------------------------===//
// StartGroupAction
//===----------------------------------------------------------------------===//
StartGroupAction::StartGroupAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::StartGroup, Printer) {}

bool StartGroupAction::activate(InputBuilder &pBuilder) {
  pBuilder.enterGroup();
  return true;
}

//===----------------------------------------------------------------------===//
// EndGroupAction
//===----------------------------------------------------------------------===//
EndGroupAction::EndGroupAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::EndGroup, Printer) {}

bool EndGroupAction::activate(InputBuilder &pBuilder) {
  pBuilder.exitGroup();
  return true;
}

//===----------------------------------------------------------------------===//
// WholeArchiveAction
//===----------------------------------------------------------------------===//
WholeArchiveAction::WholeArchiveAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::WholeArchive, Printer) {}

bool WholeArchiveAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().setWholeArchive();
  return true;
}

//===----------------------------------------------------------------------===//
// NoWholeArchiveAction
//===----------------------------------------------------------------------===//
NoWholeArchiveAction::NoWholeArchiveAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoWholeArchive, Printer) {}

bool NoWholeArchiveAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().unsetWholeArchive();
  return true;
}

//===----------------------------------------------------------------------===//
// AsNeededAction
//===----------------------------------------------------------------------===//
AsNeededAction::AsNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::AsNeeded, Printer) {}

bool AsNeededAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().setAsNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// NoAsNeededAction
//===----------------------------------------------------------------------===//
NoAsNeededAction::NoAsNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoAsNeeded, Printer) {}

bool NoAsNeededAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().unsetAsNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// AddNeededAction
//===----------------------------------------------------------------------===//
AddNeededAction::AddNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::AddNeeded, Printer) {}

bool AddNeededAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().setAddNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// NoAddNeededAction
//===----------------------------------------------------------------------===//
NoAddNeededAction::NoAddNeededAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::NoAddNeeded, Printer) {}

bool NoAddNeededAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().unsetAddNeeded();
  return true;
}

//===----------------------------------------------------------------------===//
// BDynamicAction
//===----------------------------------------------------------------------===//
BDynamicAction::BDynamicAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::BDynamic, Printer) {}

bool BDynamicAction::activate(InputBuilder &pBuilder) {
  // For partial links prefer archive libraries over shared objects
  if (pBuilder.getLinkerConfig().isLinkPartial())
    pBuilder.getAttributes().setStatic();
  else
    pBuilder.getAttributes().setDynamic();
  return true;
}

//===----------------------------------------------------------------------===//
// BStaticAction
//===----------------------------------------------------------------------===//
BStaticAction::BStaticAction(DiagnosticPrinter *Printer)
    : InputAction(InputAction::BStatic, Printer) {}

bool BStaticAction::activate(InputBuilder &pBuilder) {
  pBuilder.getAttributes().setStatic();
  return true;
}

//===----------------------------------------------------------------------===//
// DefSymAction
//===----------------------------------------------------------------------===//
DefSymAction::DefSymAction(std::string pAssignment, DiagnosticPrinter *Printer)
    : InputAction(InputAction::DefSym, Printer), m_Assignment(pAssignment) {}

// FIXME: add an assignment of type DefSym. This is so strange to create a
// Defsym Expression and create an input for it.
bool DefSymAction::activate(InputBuilder &pBuilder) {
  std::string fileName =
      (llvm::Twine("Expression(Defsym)") + llvm::Twine(m_Assignment)).str();
  Input *input = pBuilder.createInputNode(fileName, true /*isSpecial*/);
  input->setInputType(Input::Script);
  input->setResolvedPath(fileName);
  m_Assignment.append(";");
  pBuilder.setMemory(*input, &m_Assignment[0], m_Assignment.size());
  return true;
}

InputFormatAction::InputFormatAction(const std::string &inputFormat,
                                     DiagnosticPrinter *printer)
    : InputAction(InputAction::InputActionKind::InputFormat, printer),
      m_InputFormat(inputFormat) {}

bool InputFormatAction::activate(InputBuilder &builder) {
  if (m_InputFormat == "binary")
    builder.getAttributes().setIsBinary(true);
  else if (m_InputFormat == "default")
    builder.getAttributes().setIsBinary(false);
  else {
    builder.getDiagEngine()->raise(diag::error_invalid_input_format)
        << m_InputFormat;
    return false;
  }
  return true;
}
