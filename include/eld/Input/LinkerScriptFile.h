//===- LinkerScriptFile.h--------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===- LinkerScriptFile.h -------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#ifndef ELD_INPUT_LINKERSCRIPTFILE_H
#define ELD_INPUT_LINKERSCRIPTFILE_H

#include "eld/Input/InputFile.h"
#include "eld/Script/ScriptFile.h"

#include <vector>

namespace eld {

class Node;
class ScriptFile;

class LinkerScriptFile : public InputFile {
public:
  LinkerScriptFile(Input *I, DiagnosticEngine *DiagEngine);

  /// Casting support.
  static bool classof(const InputFile *I) {
    return I->getKind() == GNULinkerScriptKind;
  }

  bool isParsed() const { return Parsed; }

  void setParsed() { Parsed = true; }

  // -------------- process assignments in link order------------
  bool isAssignmentsProcessed() const { return ProcessAssignments; }

  void processAssignments();

  // -----------------------Linkerscript support -------------------------------
  std::vector<Node *> const &getNodes() { return Nodes; }

  void addNode(Node *N) { Nodes.push_back(N); }

  // ----------------------Parsed LinkerScript -----------------------
  void setScriptFile(ScriptFile *S) { Script = S; }

  ScriptFile *getScript() const { return Script; }

private:
  ScriptFile *Script = nullptr;
  std::vector<Node *> Nodes;
  bool Parsed = false;
  bool ProcessAssignments = false;
};

} // namespace eld

#endif // ELD_INPUT_LINKERSCRIPTFILE_H
