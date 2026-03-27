//===- Driver.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Driver/Driver.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Diagnostics/DiagnosticInfos.h"
#include "eld/Driver/ARMLinkDriver.h"
#include "eld/Driver/GnuLdDriver.h"
#include "eld/Driver/HexagonLinkDriver.h"
#include "eld/Driver/RISCVLinkDriver.h"
#include "eld/Driver/TemplateLinkDriver.h"
#include "eld/Driver/x86_32LinkDriver.h"
#include "eld/Driver/x86_64LinkDriver.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Support/Memory.h"
#include "eld/Support/ReproduceTarReader.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/TargetSelect.h"
#include "eld/Target/TargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/StringSaver.h"
#include <optional>

// If a command line option starts with "@", the driver reads its suffix as a
// file, parse its contents as a list of command line options, and insert them
// at the original @file position. If file cannot be read, @file is not expanded
// and left unmodified. @file can appear in a response file, so it's a recursive
// process.
static llvm::ArrayRef<const char *>
maybeExpandResponseFiles(llvm::ArrayRef<const char *> Args,
                         llvm::BumpPtrAllocator &Alloc) {
  // Expand response files.
  llvm::SmallVector<const char *, 256> SmallVec(Args.begin(), Args.end());
  llvm::StringSaver Saver(Alloc);
  llvm::cl::ExpandResponseFiles(Saver, llvm::cl::TokenizeGNUCommandLine,
                                SmallVec);

  const char **Copy = Alloc.Allocate<const char *>(SmallVec.size() + 1);
  std::copy(SmallVec.begin(), SmallVec.end(), Copy);
  Copy[SmallVec.size()] = nullptr;
  return llvm::ArrayRef(Copy, SmallVec.size());
}

static bool startsWithReplayOption(llvm::StringRef Arg) {
  return Arg == "--replay" || Arg.starts_with("--replay=");
}

static llvm::ArrayRef<const char *>
maybeExpandReplayTarball(llvm::ArrayRef<const char *> Args,
                         llvm::BumpPtrAllocator &Alloc, bool &Ok,
                         std::optional<eld::ReproduceTarReader> &TarReader) {
  Ok = true;
  TarReader.reset();
  size_t ReplayOptionIndex = 0;
  size_t ReplayValueIndex = 0;
  std::string TarPath;
  bool FoundReplay = false;
  for (size_t Index = 1; Index < Args.size() && Args[Index]; ++Index) {
    llvm::StringRef Arg = Args[Index];
    if (!startsWithReplayOption(Arg))
      continue;
    if (FoundReplay) {
      Ok = false;
      return Args;
    }
    FoundReplay = true;
    ReplayOptionIndex = Index;
    if (Arg == "--replay") {
      if (Index + 1 >= Args.size() || !Args[Index + 1]) {
        Ok = false;
        return Args;
      }
      ReplayValueIndex = Index + 1;
      TarPath = Args[ReplayValueIndex];
    } else {
      ReplayValueIndex = Index;
      TarPath = Arg.substr(strlen("--replay=")).str();
    }
  }
  if (!FoundReplay || TarPath.empty())
    return Args;

  auto TarReaderOrErr = eld::ReproduceTarReader::create(TarPath);
  if (!TarReaderOrErr) {
    llvm::errs() << "error: failed to read replay tarball: " << TarPath << "\n";
    Ok = false;
    return Args;
  }
  auto ResponseOrErr = TarReaderOrErr->readResponseFile();
  if (!ResponseOrErr) {
    llvm::errs() << "error: replay tarball is missing or invalid response.txt: "
                 << TarPath << "\n";
    Ok = false;
    return Args;
  }

  TarReader.emplace(std::move(*TarReaderOrErr));
  llvm::StringSaver Saver(Alloc);
  llvm::SmallVector<const char *, 256> ReplayTokens;
  llvm::cl::TokenizeGNUCommandLine(*ResponseOrErr, Saver, ReplayTokens);

  llvm::SmallVector<const char *, 256> Merged;
  Merged.push_back(Args[0]);
  size_t Start = 0;
  if (!ReplayTokens.empty()) {
    llvm::StringRef First = ReplayTokens.front();
    if (First.ends_with("ld.eld") || First.ends_with("eld"))
      Start = 1;
  }
  if (Start < ReplayTokens.size()) {
    Merged.push_back("--__replay_begin");
    Merged.append(ReplayTokens.begin() + Start, ReplayTokens.end());
    Merged.push_back("--__replay_end");
  }
  for (size_t Index = 1; Index < Args.size() && Args[Index]; ++Index) {
    if (Index == ReplayOptionIndex || Index == ReplayValueIndex)
      continue;
    Merged.push_back(Args[Index]);
  }

  const char **Copy = Alloc.Allocate<const char *>(Merged.size() + 1);
  std::copy(Merged.begin(), Merged.end(), Copy);
  Copy[Merged.size()] = nullptr;
  return llvm::ArrayRef(Copy, Merged.size());
}

int Driver::main(int Argc, const char **Argv) {
  // Standard set up, so program fails gracefully.
  llvm::BumpPtrAllocator Alloc;
  llvm::sys::PrintStackTraceOnErrorSignal(Argv[0]);
  llvm::PrettyStackTraceProgram StackPrinter(Argc, Argv);
  llvm::llvm_shutdown_obj Shutdown;

  llvm::ArrayRef<const char *> Args =
      maybeExpandResponseFiles({Argv, Argv + Argc}, Alloc);

  bool ReplayOk = true;
  std::optional<eld::ReproduceTarReader> TarReader;
  Args = maybeExpandReplayTarball(Args, Alloc, ReplayOk, TarReader);
  if (!ReplayOk)
    return LINK_FAIL;
  if (TarReader)
    Args = maybeExpandResponseFiles(Args, Alloc);

  Driver TheDriver;
  if (!TheDriver.setDriverFlavorAndInferredArchFromLinkCommand(Args))
    return LINK_FAIL;

  if (TarReader)
    TheDriver.getConfig().setReproduceTarReader(&*TarReader);
  return TheDriver.getLinkerDriver()->link(Args);
}

Driver::Driver(DriverFlavor F)
    : DiagEngine(new eld::DiagnosticEngine(shouldColorize())),
      Config(DiagEngine), m_DriverFlavor(F) {
  std::unique_ptr<eld::DiagnosticInfos> DiagInfo =
      std::make_unique<eld::DiagnosticInfos>(Config);
  DiagEngine->setInfoMap(std::move(DiagInfo));
}

Driver::~Driver() { delete DiagEngine; }

GnuLdDriver *Driver::getLinkerDriver() {
  if (!m_SupportedTargets.size())
    InitTarget();
  GnuLdDriver *LinkDriver = nullptr;
  switch (m_DriverFlavor) {
  case DriverFlavor::Hexagon:
  case DriverFlavor::ARM_AArch64:
  case DriverFlavor::RISCV32_RISCV64:
  case DriverFlavor::Template:
  case DriverFlavor::Unknown:
  case DriverFlavor::x86_64:
  case DriverFlavor::x86_32: {
    LinkDriver = GnuLdDriver::Create(Config, m_DriverFlavor,
                                     InferredArchFromProgramName);
    break;
  }
  case DriverFlavor::Invalid:
    return nullptr;
  }
  LinkDriver->setSupportedTargets(m_SupportedTargets);
  return LinkDriver;
}

// Initialize enabled targets.
void Driver::InitTarget() {
#ifdef LINK_POLLY_INTO_TOOLS
  llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
  polly::initializePollyPasses(Registry);
#endif

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Register all eld targets, linkers, emulation, diagnostics.
  eld::InitializeAllTargets();
  eld::InitializeAllEmulations();

  for (auto &target : eld::TargetRegistry::targets())
    m_SupportedTargets.push_back(std::string(target->name()));
}

DriverFlavor Driver::getDriverFlavorFromTarget(llvm::StringRef Target) const {
  return llvm::StringSwitch<DriverFlavor>(Target)
      .CaseLower("hexagon", DriverFlavor::Hexagon)
      .CaseLower("arm", DriverFlavor::ARM_AArch64)
      .CaseLower("aarch64", DriverFlavor::ARM_AArch64)
      .CaseLower("riscv", DriverFlavor::RISCV32_RISCV64)
      .CaseLower("template", DriverFlavor::Template)
      .CaseLower("x86_64", DriverFlavor::x86_64)
      .Default(Invalid);
}

std::vector<llvm::StringRef> Driver::getELDFlagsArgs() {
  std::optional<std::string> ELDFlags = llvm::sys::Process::GetEnv("ELDFLAGS");
  if (!ELDFlags)
    return {};

  std::string buf;
  std::stringstream ss(ELDFlags.value());

  std::vector<llvm::StringRef> ELDFlagsArgs;

  while (ss >> buf)
    ELDFlagsArgs.push_back(eld::Saver.save(buf.c_str()));
  return ELDFlagsArgs;
}

bool Driver::shouldColorize() {
  const char *term = getenv("TERM");
  return term && (0 != strcmp(term, "dumb")) &&
         llvm::sys::Process::StandardOutIsDisplayed();
}

bool Driver::setDriverFlavorAndInferredArchFromLinkCommand(
    llvm::ArrayRef<const char *> Args) {
  auto ExpDriverFlavorAndInferredArch = getDriverFlavorFromLinkCommand(Args);
  if (!ExpDriverFlavorAndInferredArch) {
    DiagEngine->raiseDiagEntry(
        std::move(ExpDriverFlavorAndInferredArch.error()));
    return false;
  }
  auto DriverFlavorAndInferredArch = ExpDriverFlavorAndInferredArch.value();
  m_DriverFlavor = DriverFlavorAndInferredArch.first;
  InferredArchFromProgramName = DriverFlavorAndInferredArch.second;
  return true;
}

eld::Expected<std::pair<DriverFlavor, std::string>>
Driver::getDriverFlavorFromLinkCommand(llvm::ArrayRef<const char *> Args) {
  auto DriverFlavorAndInferredArch =
      Driver::parseDriverFlavorFromProgramName(Args[0]);
  if ((DriverFlavorAndInferredArch.first != DriverFlavor::Invalid) &&
      (DriverFlavorAndInferredArch.first != DriverFlavor::Unknown))
    return DriverFlavorAndInferredArch;

  // We read the emulation options here to just select the driver.
  // Emulation options are properly handled by the driver.
  // Thus, the flavor selected here might not be accurate. But that's
  // alright as long as the right driver is selected. For example,
  // we set the DriverFlavor to DriverFlavor::RISCV32_RISCV64 for both riscv32
  // and riscv64 emulations. It is fine because RISCVLinDriver will see the
  // emulation options for riscv64 and properly set the emulation to riscv64.
  OPT_GnuLdOptTable Table;
  unsigned MissingIndex;
  unsigned MissingCount;
  llvm::opt::InputArgList ArgList =
      Table.ParseArgs(Args.slice(1), MissingIndex, MissingCount);
  DriverFlavor F = DriverFlavor::Unknown;
  std::string InferredArch;
  if (llvm::opt::Arg *Arg = ArgList.getLastArg(OPT_GnuLdOptTable::emulation)) {
    std::string Emulation = Arg->getValue();
#if defined(ELD_ENABLE_TARGET_HEXAGON)
    if (HexagonLinkDriver::isValidEmulation(Emulation)) {
      F = DriverFlavor::Hexagon;
      InferredArch = HexagonLinkDriver::getInferredArch(Emulation);
    } else
#endif
#if defined(ELD_ENABLE_TARGET_RISCV)
      // It is okay to consider RISCV64 emulation as RISCV32 flavor
      // here because RISCVLinkDriver will properly set the emulation.
      if (RISCVLinkDriver::isValidEmulation(Emulation)) {
        F = DriverFlavor::RISCV32_RISCV64;
        InferredArch = RISCVLinkDriver::getInferredArch(Emulation);
      } else
#endif
#if defined(ELD_ENABLE_TARGET_TEMPLATE)
          if (TemplateLinkDriver::isValidEmulation(Emulation)) {
        F = DriverFlavor::Template;
        InferredArch = TemplateLinkDriver::getInferredArch(Emulation);
      } else
#endif
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
          if (ARMLinkDriver::isValidEmulation(Emulation)) {
        F = DriverFlavor::ARM_AArch64;
        InferredArch = ARMLinkDriver::getInferredArch(Emulation);
      } else
#endif
#if defined(ELD_ENABLE_TARGET_X86)
          if (x86_64LinkDriver::isValidEmulation(Emulation)) {
        F = DriverFlavor::x86_64;
        InferredArch = x86_64LinkDriver::getInferredArch(Emulation);
      } else if (x86_32LinkDriver::isValidEmulation(Emulation)) {
        F = DriverFlavor::x86_32;
        InferredArch = x86_32LinkDriver::getInferredArch(Emulation);
      } else
#endif
        return std::make_unique<eld::DiagnosticEntry>(
            eld::Diag::fatal_unsupported_emulation,
            std::vector<std::string>{Emulation});
    return std::pair<DriverFlavor, std::string>{F, InferredArch};
  }
  if (llvm::opt::Arg *Arg = ArgList.getLastArg(OPT_GnuLdOptTable::march)) {
    std::string MachineArch = Arg->getValue();
    InferredArch = MachineArch;
#if defined(ELD_ENABLE_TARGET_HEXAGON)
    if (HexagonLinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::Hexagon;
#endif
#if defined(ELD_ENABLE_TARGET_RISCV)
    // It is okay to consider RISCV64 emulation as RISCV32 flavor
    // here because RISCVLinkDriver will properly set the emulation.
    if (RISCVLinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::RISCV32_RISCV64;
#endif
#if defined(ELD_ENABLE_TARGET_TEMPLATE)
    // It is okay to consider RISCV64 emulation as RISCV32 flavor
    // here because RISCVLinkDriver will properly set the emulation.
    if (TemplateLinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::Template;
#endif
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
    if (ARMLinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::ARM_AArch64;
#endif
#if defined(ELD_ENABLE_TARGET_X86)
    if (x86_64LinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::x86_64;
    else if (x86_32LinkDriver::isMyArch(MachineArch))
      F = DriverFlavor::x86_32;
#endif
    if (F == DriverFlavor::Invalid)
      return std::make_unique<eld::DiagnosticEntry>(
          eld::Diag::fatal_unsupported_emulation,
          std::vector<std::string>{MachineArch});
  }
  return std::pair<DriverFlavor, std::string>{F, InferredArch};
}

std::pair<DriverFlavor, std::string>
Driver::parseDriverFlavorFromProgramName(const char *argv0) {
  // Deduct the flavor from argv[0].
  llvm::StringRef ProgramName = llvm::sys::path::filename(argv0);
  if (ProgramName.ends_with_insensitive(".exe"))
    ProgramName = ProgramName.drop_back(4);
  std::pair<DriverFlavor, std::string> DriverPair;
  DriverPair =
      llvm::StringSwitch<std::pair<DriverFlavor, std::string>>(ProgramName)
          .StartsWith("hexagon", std::make_pair(DriverFlavor::Hexagon,
                                                std::string("hexagon")))
          .StartsWith("arm", std::make_pair(DriverFlavor::ARM_AArch64,
                                            std::string("arm")))
          .StartsWith("aarch64", std::make_pair(DriverFlavor::ARM_AArch64,
                                                std::string("aarch64")))
          .StartsWith("riscv", std::make_pair(DriverFlavor::RISCV32_RISCV64,
                                              std::string("riscv32")))
          .StartsWith("template", std::make_pair(DriverFlavor::Template,
                                                 std::string("template")))
          .StartsWith("x86_64", std::make_pair(DriverFlavor::x86_64,
                                               std::string("x86_64")))
          .Default(std::make_pair(DriverFlavor::Invalid, ""));
  return DriverPair;
}
