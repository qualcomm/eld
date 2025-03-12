//===- InputBuilder.h------------------------------------------------------===//
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
#ifndef ELD_INPUT_INPUTBUILDER_H
#define ELD_INPUT_INPUTBUILDER_H

#include "eld/Input/Input.h"
#include "eld/Input/InputAction.h"
#include "eld/Input/InputTree.h"
#include "eld/Script/WildcardPattern.h"
#include "eld/Support/MemoryArea.h"
#include "llvm/ADT/StringSet.h"
#include <string>
#include <vector>

namespace eld {

class LinkerConfig;
class AttrConstraint;
class DiagnosticEngine;

/** \class InputBuilder
 *  \brief InputBuilder recieves InputActions and build the InputTree.
 *
 *  InputBuilder build input tree and inputs.
 */
class InputBuilder {
private:
  InputBuilder() = delete;

public:
  typedef std::vector<Node *>::const_iterator InputIteratorT;

  explicit InputBuilder(const LinkerConfig &pConfig);

  explicit InputBuilder(const LinkerConfig &pConfig, const Attribute &attr);

  virtual ~InputBuilder();

  // -----  input operations  ----- //
  Input *createInput(const std::string pName, Input::Type T = Input::Default);

  Input *createDeferredInput(const std::string &pName, Input::Type T);

  Input *createInputNode(std::string Name, bool isSpecial = false);

  bool setMemory(Input &pInput, void *pMemBuffer, size_t pSize);

  void enterGroup();

  void exitGroup();

  void makeBStatic();

  Attribute &getAttributes() { return m_Attr; }

  std::vector<Node *> const &getInputs() { return Tree; }

  std::vector<Node *> &getTree() { return Tree; }

  DiagnosticEngine *getDiagEngine() const;

  const LinkerConfig &getLinkerConfig() const { return m_Config; }

private:
  std::vector<Node *> Tree;
  Attribute m_Attr;
  const LinkerConfig &m_Config;
};

} // end of namespace eld

#endif
