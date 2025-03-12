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
#include <vector>

namespace eld {

class Node;

class LinkerScriptFile : public InputFile {
public:
  LinkerScriptFile(Input *I, DiagnosticEngine *diagEngine);

  /// Casting support.
  static bool classof(const InputFile *I) {
    return I->getKind() == GNULinkerScriptKind;
  }

  bool isParsed() const { return m_Parsed; }

  void setParsed() { m_Parsed = true; }

  // -----------------------Linkerscript support -------------------------------
  std::vector<Node *> const &getNodes() { return m_Nodes; }

  void addNode(Node *N) { m_Nodes.push_back(N); }

private:
  std::vector<Node *> m_Nodes;
  bool m_Parsed = false;
};

} // namespace eld

#endif // ELD_INPUT_LINKERSCRIPTFILE_H
