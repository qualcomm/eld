//===- InputTree.h---------------------------------------------------------===//
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
#ifndef ELD_INPUT_INPUTTREE_H
#define ELD_INPUT_INPUTTREE_H

#include <cstdint>

namespace eld {

class Input;

class Node {
public:
  enum Kind { File, GroupStart, GroupEnd, LibStart, LibEnd };

  Node(Kind K) : NodeKind(K) {}

  Kind kind() const { return NodeKind; }

private:
  Kind NodeKind;
};

class Attribute {
public:
  // -----  modifiers  ----- //
  void setWholeArchive() { WholeArchive = true; }

  void unsetWholeArchive() { WholeArchive = false; }

  void setAsNeeded() { AsNeeded = true; }

  void unsetAsNeeded() { AsNeeded = false; }

  void setAddNeeded() { AddNeeded = true; }

  void unsetAddNeeded() { AddNeeded = false; }

  void setStatic() { Static = true; }

  void setDynamic() { Static = false; }

  void setJustSymbols() { JustSymbols = true; }

  void unsetJustSymbols() { JustSymbols = false; }

  bool isWholeArchive() const { return WholeArchive; }

  bool isAsNeeded() const { return AsNeeded; }

  bool isAddNeeded() const { return AddNeeded; }

  bool isStatic() const { return Static; }

  bool isDynamic() const { return !Static; }

  bool isJustSymbols() const { return JustSymbols; }

  void setIsBinary(bool pIsBinary = true) { IsBinary = true; }

  bool isBinary() const { return IsBinary; }

  void setPatchBase(bool Value = true) { PatchBase = Value; }

  bool isPatchBase() const { return PatchBase; }

  Attribute &operator=(const Attribute &) = default;

  Attribute(const Attribute &) = default;

  Attribute()
      : WholeArchive(false), AsNeeded(false), AddNeeded(false), Static(false),
        JustSymbols(false), IsBinary(false), PatchBase(false) {}

private:
  // FIXME: Convert to std::optional<bool>
  bool WholeArchive;
  bool AsNeeded;
  bool AddNeeded;
  bool Static;
  bool JustSymbols;
  bool IsBinary;
  bool PatchBase;
};

// -----  comparisons  ----- //
inline bool operator==(const Attribute &PLhs, const Attribute &PRhs) {
  return ((PLhs.isWholeArchive() == PRhs.isWholeArchive()) &&
          (PLhs.isAsNeeded() == PRhs.isAsNeeded()) &&
          (PLhs.isAddNeeded() == PRhs.isAddNeeded()) &&
          (PLhs.isStatic() == PRhs.isStatic()) &&
          (PLhs.isJustSymbols() == PRhs.isJustSymbols()));
}

inline bool operator!=(const Attribute &PLhs, const Attribute &PRhs) {
  return !(PLhs == PRhs);
}

class FileNode : public Node {
public:
  FileNode(Input *I) : Node(Node::File), In(I) {}

  static bool classof(const Node *N) { return (N->kind() == Node::File); }

  Input *getInput() const { return In; }

private:
  Input *In = nullptr;
};

class GroupStart : public Node {
public:
  GroupStart() : Node(Node::GroupStart) {}

  static bool classof(const Node *N) { return (N->kind() == Node::GroupStart); }
};

class GroupEnd : public Node {
public:
  GroupEnd() : Node(Node::GroupEnd) {}

  static bool classof(const Node *N) { return (N->kind() == Node::GroupEnd); }
};

class LibStart : public Node {
public:
  explicit LibStart(const Attribute &Attr, bool IsThin, uint64_t Id)
      : Node(Node::LibStart), Attr(Attr), IsThin(IsThin), Id(Id) {}

  const Attribute &getAttribute() const { return Attr; }

  bool isThin() const { return IsThin; }

  uint64_t getId() const { return Id; }

  static bool classof(const Node *N) { return (N->kind() == Node::LibStart); }

private:
  Attribute Attr;
  bool IsThin = false;
  uint64_t Id = 0;
};

class LibEnd : public Node {
public:
  LibEnd() : Node(Node::LibEnd) {}

  static bool classof(const Node *N) { return (N->kind() == Node::LibEnd); }
};
} // namespace eld

#endif
