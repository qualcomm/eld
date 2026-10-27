//===- ICF.h---------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_CORE_ICF_H
#define ELD_CORE_ICF_H

namespace eld {
class LinkerConfig;
class Module;

/// The initial identical code folding pass. It intentionally accepts only
/// relocation-free sections for now. Relocation equivalence support will be
/// added later.
class ICF {
public:
  ICF(Module &Module, LinkerConfig &Config)
      : ThisModule(Module), ThisConfig(Config) {}
  void run();

private:
  Module &ThisModule;
  LinkerConfig &ThisConfig;
};
} // namespace eld

#endif
