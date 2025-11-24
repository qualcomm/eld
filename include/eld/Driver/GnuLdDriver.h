//===- GnuLdDriver.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
#ifndef ELD_DRIVER_GNULDDRIVER_H
#define ELD_DRIVER_GNULDDRIVER_H

#include "eld/Config/Config.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Config/Version.h"
#include "eld/Core/LinkerScript.h"
#include "eld/Core/Module.h"
#include "eld/Driver/Driver.h"
#include "eld/Input/InputAction.h"
#include "eld/Script/ScriptAction.h"
#include "eld/Support/OutputTarWriter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/TargetParser/Host.h"
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace llvm {
namespace lto {
struct Config;
}
} // namespace llvm

// Forward declarations.
namespace eld {
class DiagnosticEngine;
class Module;
class LinkerScript;
} // namespace eld

// Create OptTable class for parsing actual command line arguments
class OPT_GnuLdOptTable : public llvm::opt::GenericOptTable {
public:
  enum {
    INVALID = 0,
#define OPTION(PREFIXES_OFFSET, PREFIXED_NAME_OFFSET, ID, KIND, GROUP, ALIAS,  \
               ALIASARGS, FLAGS, VISIBILITY, PARAM, HELPTEXT,                  \
               HELPTEXTSFORVARIANTS, METAVAR, VALUES, SUBCOMMANDIDS_OFFSET)                          \
  ID,
#include "eld/Driver/GnuLinkerOptions.inc"
#undef OPTION
  };

  OPT_GnuLdOptTable();
};

class DLL_A_EXPORT GnuLdDriver {
public:
  static GnuLdDriver *Create(eld::LinkerConfig &C, DriverFlavor F,
                             std::string InferredArchFromProgramName);

  GnuLdDriver(eld::LinkerConfig &C, DriverFlavor F = DriverFlavor::Invalid);

  static GnuLdDriver *Create(eld::LinkerConfig &C, uint8_t Machine,
                             bool is64bit);

  virtual ~GnuLdDriver();

  int link(llvm::ArrayRef<const char *> Args);

  // Main entry point.
  virtual int link(llvm::ArrayRef<const char *> Args,
                   llvm::ArrayRef<llvm::StringRef> ELDFlagsArgs);

  virtual std::optional<int> parseOptions(llvm::ArrayRef<const char *> ArgsArr,
                                          llvm::opt::InputArgList &ArgList);

  // Driver-specific function to call the template processLTOOptions with the
  // correct options table.
  virtual bool processLTOOptions(llvm::lto::Config &Conf,
                                 std::vector<std::string> &LLVMOptions);

protected:
  // Check if the options are invalid.
  template <class T> bool checkOptions(llvm::opt::InputArgList &Args) const;

  // Process the linker options.
  template <class T> bool processOptions(llvm::opt::InputArgList &Args);

  // Process LLVM options.
  template <class T>
  bool processLLVMOptions(llvm::opt::InputArgList &Args) const;

  // Process Target specific options.
  template <class T> bool processTargetOptions(llvm::opt::InputArgList &Args);

  // Override Options
  template <class T> bool overrideOptions(llvm::opt::InputArgList &Args);

  // process options for --reproduce response
  template <class T>
  bool processReproduceOption(llvm::opt::InputArgList &Args,
                              eld::OutputTarWriter *outputTar,
                              std::vector<eld::InputAction *> &actions);

  // Parse LTO options and update the LTO config object.
  template <typename OptTable>
  bool processLTOOptions(llvm::lto::Config &Conf,
                         std::vector<std::string> &LLVMOptions);

public:
  // Writes reproduce tarball to disk
  static void writeReproduceTar(void *ModulePtr);

  // Default signal handler for linker issues
  static void defaultSignalHandler(void *cookie);

  // if the users presses ctrl+c or ctrl+t
  static void ReproduceInterruptHandler();

  template <class T>
  bool createInputActions(llvm::opt::InputArgList &Args,
                          std::vector<eld::InputAction *> &actions);

  // Do the real link.
  template <class T>
  bool doLink(llvm::opt::InputArgList &Args,
              std::vector<eld::InputAction *> &action);

  // Handle reproduce option
  template <class T>
  bool handleReproduce(llvm::opt::InputArgList &Args,
                       std::vector<eld::InputAction *> &action, bool);

  // Emit stats
  bool emitStats(eld::Module &M) const;

  // Supported Targets
  void setSupportedTargets(std::vector<std::string> &Targets) {
    m_SupportedTargets = Targets;
  }

  std::vector<const char *>
  getAllArgs(const std::vector<const char *> &allArgs,
             const std::vector<llvm::StringRef> &ELDFlagsArgs) const;

protected:
  int getInteger(llvm::opt::InputArgList &Args, unsigned Key,
                 int Default) const;
  uint32_t getUnsignedInteger(llvm::opt::Arg *arg, uint32_t Default) const;

  bool hasZOption(llvm::opt::InputArgList &Args, llvm::StringRef Key) const;
  const char *getLtoStatus() const;

  // print more information about the linker
  void printAboutInfo() const;

  // print version information
  void printVersionInfo() const;

  // Print repository revision information of both ELD and llvm-project.
  void printRepositoryVersion() const;

  // Returns the driver's flavor name.
  std::string getDriverFlavorName() const;

  const std::string &getProgramName() const { return LinkerProgramName; }

  bool isDriverFlavorValid() const {
    return m_DriverFlavor != DriverFlavor::Invalid;
  }

  bool isDriverFlavorUnknown() const {
    return m_DriverFlavor == DriverFlavor::Unknown;
  }

  void setDriverFlavor(DriverFlavor F) { m_DriverFlavor = F; }

private:
  // Raise diag entry for trace.
  bool checkAndRaiseTraceDiagEntry(eld::Expected<void> E) const;

protected:
  eld::LinkerConfig &Config;
  eld::DiagnosticEngine *DiagEngine;
  eld::LinkerScript m_Script;
  static eld::Module *ThisModule;
  llvm::opt::OptTable *Table;
  std::once_flag once_flag;
  DriverFlavor m_DriverFlavor;
  std::vector<std::string> m_SupportedTargets;
  std::string LinkerProgramName;
};

#endif
