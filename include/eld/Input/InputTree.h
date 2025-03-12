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

namespace eld {

class Input;

class Node {
public:
  enum Kind { File, GroupStart, GroupEnd };

  Node(Kind k) : NodeKind(k) {}

  Kind kind() const { return NodeKind; }

private:
  Kind NodeKind;
};

class Attribute {
public:
  // -----  modifiers  ----- //
  void setWholeArchive() { m_WholeArchive = true; }

  void unsetWholeArchive() { m_WholeArchive = false; }

  void setAsNeeded() { m_AsNeeded = true; }

  void unsetAsNeeded() { m_AsNeeded = false; }

  void setAddNeeded() { m_AddNeeded = true; }

  void unsetAddNeeded() { m_AddNeeded = false; }

  void setStatic() { m_Static = true; }

  void setDynamic() { m_Static = false; }

  void setJustSymbols() { m_JustSymbols = true; }

  void unsetJustSymbols() { m_JustSymbols = false; }

  bool isWholeArchive() const { return m_WholeArchive; }

  bool isAsNeeded() const { return m_AsNeeded; }

  bool isAddNeeded() const { return m_AddNeeded; }

  bool isStatic() const { return m_Static; }

  bool isDynamic() const { return !m_Static; }

  bool isJustSymbols() const { return m_JustSymbols; }

  void setIsBinary(bool isBinary = true) { m_IsBinary = true; }

  bool isBinary() const { return m_IsBinary; }

  void setPatchBase(bool Value = true) { m_PatchBase = Value; }

  bool isPatchBase() const { return m_PatchBase; }

  Attribute &operator=(const Attribute &) = default;

  Attribute(const Attribute &) = default;

  Attribute()
      : m_WholeArchive(false), m_AsNeeded(false), m_AddNeeded(false),
        m_Static(false), m_JustSymbols(false), m_IsBinary(false),
        m_PatchBase(false) {}

private:
  // FIXME: Convert to std::optional<bool>
  bool m_WholeArchive;
  bool m_AsNeeded;
  bool m_AddNeeded;
  bool m_Static;
  bool m_JustSymbols;
  bool m_IsBinary;
  bool m_PatchBase;
};

// -----  comparisons  ----- //
inline bool operator==(const Attribute &pLHS, const Attribute &pRHS) {
  return ((pLHS.isWholeArchive() == pRHS.isWholeArchive()) &&
          (pLHS.isAsNeeded() == pRHS.isAsNeeded()) &&
          (pLHS.isAddNeeded() == pRHS.isAddNeeded()) &&
          (pLHS.isStatic() == pRHS.isStatic()) &&
          (pLHS.isJustSymbols() == pRHS.isJustSymbols()));
}

inline bool operator!=(const Attribute &pLHS, const Attribute &pRHS) {
  return !(pLHS == pRHS);
}

class FileNode : public Node {
public:
  FileNode(Input *I) : Node(Node::File), In(I) {}

  static bool classof(const Node *n) { return (n->kind() == Node::File); }

  Input *getInput() const { return In; }

private:
  Input *In = nullptr;
};

class GroupStart : public Node {
public:
  GroupStart() : Node(Node::GroupStart) {}

  static bool classof(const Node *n) { return (n->kind() == Node::GroupStart); }
};

class GroupEnd : public Node {
public:
  GroupEnd() : Node(Node::GroupEnd) {}

  static bool classof(const Node *n) { return (n->kind() == Node::GroupEnd); }
};
} // namespace eld

#endif
