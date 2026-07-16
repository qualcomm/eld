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
#include <optional>

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

  bool hasWholeArchive() const { return WholeArchive.has_value(); }

  bool hasAsNeeded() const { return AsNeeded.has_value(); }

  bool hasAddNeeded() const { return AddNeeded.has_value(); }

  bool hasStatic() const { return Static.has_value(); }

  bool hasJustSymbols() const { return JustSymbols.has_value(); }

  bool hasIsBinary() const { return IsBinary.has_value(); }

  bool hasPatchBase() const { return PatchBase.has_value(); }

  bool isWholeArchive() const { return (hasWholeArchive() && *WholeArchive); }

  bool isAsNeeded() const { return (hasAsNeeded() && *AsNeeded); }

  bool isAddNeeded() const { return (hasAddNeeded() && *AddNeeded); }

  bool isStatic() const { return (hasStatic() && *Static); }

  bool isDynamic() const { return (hasStatic() && !*Static); }

  bool isJustSymbols() const { return (hasJustSymbols() && *JustSymbols); }

  void setIsBinary(bool pIsBinary = true) { IsBinary = true; }

  bool isBinary() const { return (hasIsBinary() && *IsBinary); }

  void setPatchBase(bool Value = true) { PatchBase = Value; }

  bool isPatchBase() const { return (hasPatchBase() && *PatchBase); }

  Attribute &operator=(const Attribute &) = default;

  Attribute(const Attribute &) = default;

  Attribute() {}

private:
  std::optional<bool> WholeArchive;
  std::optional<bool> AsNeeded;
  std::optional<bool> AddNeeded;
  std::optional<bool> Static;
  std::optional<bool> JustSymbols;
  std::optional<bool> IsBinary;
  std::optional<bool> PatchBase;
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
