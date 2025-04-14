//===- Driver.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_DRIVER_H
#define ELD_DRIVER_DRIVER_H

#include "Expected.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Driver/Flavor.h"
#include "eld/Support/Defines.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#define LINK_SUCCESS 0
#define LINK_FAIL 1

namespace eld {
class DiagnosticEngine;
}

class GnuLdDriver;

class DLL_A_EXPORT Driver {
public:
  Driver(Flavor F, std::string Triple);

  bool setFlavorAndTripleFromLinkCommand(llvm::ArrayRef<const char *> Args);

  static eld::Expected<Driver>
  createDriverForLinkCommand(llvm::ArrayRef<const char *> Args);

  GnuLdDriver *getLinker();

  virtual ~Driver();

  /// Returns a sensible default value for whether or not the colors should be
  /// used in the terminal output.
  static bool shouldColorize();

  /// Returns arguments from ELDFLAGS environment variable.
  static std::vector<llvm::StringRef> getELDFlagsArgs();

private:
  std::string getStringFromTarget(llvm::StringRef Target) const;

  Flavor getFlavorFromTarget(llvm::StringRef Target) const;

  void InitTarget();

  eld::Expected<std::pair<Flavor, std::string>>
  getFlavorAndTripleFromLinkCommand(llvm::ArrayRef<const char *> Args);

  std::pair<Flavor, std::string>
  parseFlavorAndTripleFromProgramName(const char *argv0);

protected:
  eld::DiagnosticEngine *DiagEngine = nullptr;
  eld::LinkerConfig Config;

private:
  Flavor m_Flavor = Invalid;
  std::string m_Triple;
  std::vector<std::string> m_SupportedTargets;
};

#endif
