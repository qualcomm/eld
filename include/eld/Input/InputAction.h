//===- InputAction.h-------------------------------------------------------===//
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
#ifndef ELD_INPUT_INPUTACTION_H
#define ELD_INPUT_INPUTACTION_H

#include "eld/Input/Input.h"
#include "eld/Support/Path.h"
#include <string>

namespace eld {

class InputBuilder;
class Input;

//===----------------------------------------------------------------------===//
// Base InputAction
//===----------------------------------------------------------------------===//
/** \class InputAction
 *  \brief InputAction is a command object to construct eld::InputTree.
 */
class InputAction {
public:
  enum InputActionKind {
    AddNeeded,
    AsNeeded,
    BDynamic,
    BStatic,
    DefSym,
    EndGroup,
    InputFormat,
    InputFile,
    Namespec,
    NoAddNeeded,
    NoAsNeeded,
    NoWholeArchive,
    Script,
    StartGroup,
    WholeArchive,
    JustSymbols,
  };

protected:
  explicit InputAction(InputActionKind K, DiagnosticPrinter *Printer);

public:
  virtual ~InputAction();

  virtual bool activate(InputBuilder &) = 0;

  InputActionKind getInputActionKind() const { return K; }

  // Helper functions.
  virtual bool isScript() const { return K == Script; }

  virtual Input *getInput() const { return nullptr; }

private:
  InputAction();                               // DO_NOT_IMPLEMENT
  InputAction(const InputAction &);            // DO_NOT_IMPLEMENT
  InputAction &operator=(const InputAction &); // DO_NOT_IMPLEMENT

private:
  InputActionKind K;
};

/// InputFileAction
class InputFileAction : public InputAction {
public:
  explicit InputFileAction(std::string Name, DiagnosticPrinter *Printer);

  explicit InputFileAction(std::string Name, InputAction::InputActionKind K,
                           DiagnosticPrinter *Printer);

  virtual bool activate(InputBuilder &) override;

  Input *getInput() const override { return I; }

  virtual ~InputFileAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const InputFileAction *I) { return true; }

  void setFileName(std::string FileName) { m_Name = FileName; }

protected:
  std::string m_Name;
  Input *I = nullptr;
};

/// NamespecAction
class NamespecAction : public InputAction {
public:
  NamespecAction(const std::string &pNamespec, DiagnosticPrinter *Printer);

  const std::string &namespec() const { return m_Namespec; }

  bool activate(InputBuilder &) override;

  virtual ~NamespecAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const NamespecAction *N) { return true; }

  Input *getInput() const override { return I; }

private:
  std::string m_Namespec;
  Input *I = nullptr;
};

/// StartGroupAction
class StartGroupAction : public InputAction {
public:
  explicit StartGroupAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~StartGroupAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const StartGroupAction *S) { return true; }
};

/// EndGroupAction
class EndGroupAction : public InputAction {
public:
  explicit EndGroupAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~EndGroupAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const EndGroupAction *E) { return true; }
};

/// WholeArchiveAction
class WholeArchiveAction : public InputAction {
public:
  explicit WholeArchiveAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~WholeArchiveAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const WholeArchiveAction *W) { return true; }
};

/// NoWholeArchiveAction
class NoWholeArchiveAction : public InputAction {
public:
  explicit NoWholeArchiveAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~NoWholeArchiveAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const NoWholeArchiveAction *N) { return true; }
};

/// AsNeededAction
class AsNeededAction : public InputAction {
public:
  explicit AsNeededAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~AsNeededAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const AsNeededAction *A) { return true; }
};

/// NoAsNeededAction
class NoAsNeededAction : public InputAction {
public:
  explicit NoAsNeededAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~NoAsNeededAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const NoAsNeededAction *N) { return true; }
};

/// AddNeededAction
class AddNeededAction : public InputAction {
public:
  explicit AddNeededAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~AddNeededAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const AddNeededAction *A) { return true; }
};

/// NoAddNeededAction
class NoAddNeededAction : public InputAction {
public:
  explicit NoAddNeededAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~NoAddNeededAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const NoAddNeededAction *N) { return true; }
};

/// BDynamicAction
class BDynamicAction : public InputAction {
public:
  explicit BDynamicAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~BDynamicAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const BDynamicAction *N) { return true; }
};

/// BStaticAction
class BStaticAction : public InputAction {
public:
  explicit BStaticAction(DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  virtual ~BStaticAction() {}

  static bool classof(const InputAction *I) { return true; }

  static bool classof(const BStaticAction *B) { return true; }
};

/// DefSymAction
class DefSymAction : public InputAction {
public:
  explicit DefSymAction(std::string pAssignment, DiagnosticPrinter *Printer);

  bool activate(InputBuilder &) override;

  const std::string &assignment() const { return m_Assignment; }

  virtual ~DefSymAction() {}

  // FIXME: LLVM casting will work incorrectly for DefSymAction because all
  // Input actions can use this overload.
  static bool classof(const InputAction *I) { return true; }

  static bool classof(const DefSymAction *D) { return true; }

private:
  std::string m_Assignment;
};

/// InputFormatAction handles the processing of '--format|-b <input_format>'
/// command-line options.
class InputFormatAction : public InputAction {
public:
  explicit InputFormatAction(const std::string &inputFormat,
                             DiagnosticPrinter *printer);

  bool activate(InputBuilder &) override;

  static bool classof(const InputAction *I) {
    return I->getInputActionKind() == InputActionKind::InputFormat;
  }

private:
  const std::string m_InputFormat;
};

} // namespace eld

#endif
