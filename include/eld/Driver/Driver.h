//===- Driver.h------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_DRIVER_DRIVER_H
#define ELD_DRIVER_DRIVER_H

#include "eld/Driver/Flavor.h"
#include "eld/Support/Defines.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#define LINK_SUCCESS 0
#define LINK_FAIL 1

class DLL_A_EXPORT Driver {
public:
  Driver(Flavor F, std::string Triple);

  Driver *getLinker();

  int link(llvm::ArrayRef<const char *> Args);

  virtual int link(llvm::ArrayRef<const char *> Args,
                   llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs) {
    return 0;
  }

  Driver() {}

  virtual ~Driver() {}

  const std::string &getProgramName() const { return m_LinkerProgramName; }

private:
  std::string getStringFromTarget(llvm::StringRef Target) const;

  Flavor getFlavorFromTarget(llvm::StringRef Target) const;

  void InitTarget();

  /// Returns arguments from ELDFLAGS environment variable.
  std::vector<llvm::StringRef> getELDFlagsArgs();

private:
  Flavor m_Flavor = Invalid;
  std::string m_Triple;
  std::vector<std::string> m_SupportedTargets;
  std::string m_LinkerProgramName;
};

#endif
