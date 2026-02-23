//===- ScopedScriptEvalContext.h-------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SCRIPT_SCOPEDSCRIPTEVALCONTEXT_H
#define ELD_SCRIPT_SCOPEDSCRIPTEVALCONTEXT_H

#include "eld/Core/LinkerScript.h"

namespace eld {

class ELFSection;

class ScopedScriptEvalOutputSection {
public:
  ScopedScriptEvalOutputSection(LinkerScript &Script, ELFSection *Section)
      : Script(Script) {
    Script.setCurrentOutputSectionForScriptEval(Section);
  }

  ~ScopedScriptEvalOutputSection() {
    Script.setCurrentOutputSectionForScriptEval(nullptr);
  }

  ScopedScriptEvalOutputSection(const ScopedScriptEvalOutputSection &) = delete;
  ScopedScriptEvalOutputSection &
  operator=(const ScopedScriptEvalOutputSection &) = delete;

private:
  LinkerScript &Script;
};

} // namespace eld

#endif
