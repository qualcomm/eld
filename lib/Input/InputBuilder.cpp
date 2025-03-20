//===- InputBuilder.cpp----------------------------------------------------===//
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

#include "eld/Input/InputBuilder.h"
#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Support/MemoryArea.h"
#include "eld/Support/Path.h"

using namespace eld;

InputBuilder::InputBuilder(const LinkerConfig &PConfig)
    : Attr(), Config(PConfig) {}

InputBuilder::InputBuilder(const LinkerConfig &PConfig, const Attribute &Attr)
    : Attr(Attr), Config(PConfig) {}

InputBuilder::~InputBuilder() { Tree.clear(); }

Input *InputBuilder::createInput(const std::string PName, Input::InputType T) {
  return make<Input>(PName, Attr, Config.getDiagEngine(), T);
}

Input *InputBuilder::createInputNode(std::string Name, bool IsSpecial) {
  Input *I = createInput(Name);
  Tree.push_back(make<FileNode>(I));
  return I;
}

Input *InputBuilder::createDeferredInput(const std::string &Namespec,
                                         Input::InputType T) {
  Input *I = createInput(Namespec, T);
  Tree.push_back(make<FileNode>(I));
  return I;
}

void InputBuilder::enterGroup() { Tree.push_back(make<GroupStart>()); }

void InputBuilder::exitGroup() { Tree.push_back(make<GroupEnd>()); }

bool InputBuilder::setMemory(Input &PInput, void *PMemBuffer, size_t PSize) {
  MemoryArea *MemBufCopy = MemoryArea::CreateCopy(
      llvm::StringRef(static_cast<const char *>(PMemBuffer), PSize));
  PInput.setMemArea(MemBufCopy);
  return true;
}

void InputBuilder::makeBStatic() { getAttributes().setStatic(); }

DiagnosticEngine *InputBuilder::getDiagEngine() const {
  return Config.getDiagEngine();
}
