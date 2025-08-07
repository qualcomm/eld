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
#include "eld/Driver/x86_64LinkDriver.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Support/Memory.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/TargetSelect.h"
#include "eld/Target/TargetMachine.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include <optional>

Driver::Driver(DriverFlavor F, std::string Triple)
    : DiagEngine(new eld::DiagnosticEngine(shouldColorize())),
      Config(DiagEngine), m_DriverFlavor(F), m_Triple(Triple) {
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
  case DriverFlavor::x86_64: {
    LinkDriver = GnuLdDriver::Create(Config, m_DriverFlavor, m_Triple);
    break;
  }
  default:
    break;
  }
  if (!LinkDriver)
    LinkDriver = GnuLdDriver::Create(
        Config, getDriverFlavorFromTarget(m_SupportedTargets.front()),
        m_Triple);
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

std::string Driver::getStringFromTarget(llvm::StringRef Target) const {
  return llvm::StringSwitch<std::string>(Target)
      .CaseLower("hexagon", "hexagon")
      .CaseLower("arm", "arm")
      .CaseLower("aarch64", "aarch64")
      .CaseLower("riscv32", "riscv")
      .CaseLower("riscv64", "riscv")
      .CaseLower("iu", "iu")
      .CaseLower("x86_64", "x86_64")
      .Default("");
}

DriverFlavor Driver::getDriverFlavorFromTarget(llvm::StringRef Target) const {
  return llvm::StringSwitch<DriverFlavor>(Target)
      .CaseLower("hexagon", DriverFlavor::Hexagon)
      .CaseLower("arm", DriverFlavor::ARM_AArch64)
      .CaseLower("aarch64", DriverFlavor::ARM_AArch64)
      .CaseLower("riscv", DriverFlavor::RISCV32_RISCV64)
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

bool Driver::setDriverFlavorAndTripleFromLinkCommand(
    llvm::ArrayRef<const char *> Args) {
  auto ExpDriverFlavorAndTriple = getDriverFlavorAndTripleFromLinkCommand(Args);
  if (!ExpDriverFlavorAndTriple) {
    DiagEngine->raiseDiagEntry(std::move(ExpDriverFlavorAndTriple.error()));
    return false;
  }
  auto DriverFlavorAndTriple = ExpDriverFlavorAndTriple.value();
  m_DriverFlavor = DriverFlavorAndTriple.first;
  m_Triple = DriverFlavorAndTriple.second;
  return true;
}

eld::Expected<std::pair<DriverFlavor, std::string>>
Driver::getDriverFlavorAndTripleFromLinkCommand(
    llvm::ArrayRef<const char *> Args) {
  auto DriverFlavorAndTriple =
      Driver::parseDriverFlavorAndTripleFromProgramName(Args[0]);
  if (DriverFlavorAndTriple.first != DriverFlavor::Invalid)
    return DriverFlavorAndTriple;

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
  DriverFlavor F = DriverFlavor::Invalid;
  if (llvm::opt::Arg *Arg = ArgList.getLastArg(OPT_GnuLdOptTable::emulation)) {
    std::string Emulation = Arg->getValue();
#if defined(ELD_ENABLE_TARGET_HEXAGON)
    if (HexagonLinkDriver::isValidEmulation(Emulation))
      F = DriverFlavor::Hexagon;
#endif
#if defined(ELD_ENABLE_TARGET_RISCV)
    // It is okay to consider RISCV64 emulation as RISCV32 flavor
    // here because RISCVLinkDriver will properly set the emulation.
    if (RISCVLinkDriver::isSupportedEmulation(Emulation))
      F = DriverFlavor::RISCV32_RISCV64;
#endif
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
    std::optional<llvm::Triple> optTriple =
        ARMLinkDriver::ParseEmulation(Emulation, DiagEngine);
    if (optTriple.has_value()) {
      llvm::Triple EmulationTriple = optTriple.value();
      if (EmulationTriple.getArch() == llvm::Triple::arm)
        F = DriverFlavor::ARM_AArch64;
      else if (EmulationTriple.getArch() == llvm::Triple::aarch64)
        F = DriverFlavor::ARM_AArch64;
    }
#endif
#if defined(ELD_ENABLE_TARGET_X86_64)
    if (x86_64LinkDriver::isValidEmulation(Emulation))
      F = DriverFlavor::x86_64;
#endif
    if (F == DriverFlavor::Invalid)
      return std::make_unique<eld::DiagnosticEntry>(
          eld::Diag::fatal_unsupported_emulation,
          std::vector<std::string>{Emulation});
  }
  return std::pair<DriverFlavor, std::string>{F, ""};
}

static std::string parseProgName(llvm::StringRef ProgName) {
  std::vector<llvm::StringRef> suffixes;
  llvm::StringRef altName(LINKER_ALT_NAME);
  suffixes.push_back("ld");
  suffixes.push_back("ld.eld");
  if (altName.size())
    suffixes.push_back(altName);

  for (auto suffix : suffixes) {
    if (!ProgName.ends_with(suffix))
      continue;
    llvm::StringRef::size_type LastComponent =
        ProgName.rfind('-', ProgName.size() - suffix.size());
    if (LastComponent == llvm::StringRef::npos)
      continue;
    llvm::StringRef Prefix = ProgName.slice(0, LastComponent);
    return Prefix.str();
  }
  return std::string();
}

std::pair<DriverFlavor, std::string>
Driver::parseDriverFlavorAndTripleFromProgramName(const char *argv0) {
  // Deduct the flavor from argv[0].
  llvm::StringRef ProgramName = llvm::sys::path::filename(argv0);
  if (ProgramName.ends_with_insensitive(".exe"))
    ProgramName = ProgramName.drop_back(4);
  std::string Triple;
  DriverFlavor F;
  F = llvm::StringSwitch<DriverFlavor>(ProgramName)
          .Case("hexagon-link", DriverFlavor::Hexagon)
          .Case("hexagon-linux-link", DriverFlavor::Hexagon)
          .Case("arm-link", DriverFlavor::ARM_AArch64)
          .Case("aarch64-link", DriverFlavor::ARM_AArch64)
          .Case("x86_64-link", DriverFlavor::x86_64)
          .Case("riscv-link", DriverFlavor::RISCV32_RISCV64)
          .Case("riscv32-link", DriverFlavor::RISCV32_RISCV64)
          .Case("riscv64-link", DriverFlavor::RISCV32_RISCV64)
          .Default(Invalid);
  // Try to get the DriverFlavor from the triple.
  if (F == Invalid) {
    Triple = parseProgName(ProgramName);
    if (!Triple.empty()) {
      llvm::StringRef TripleRef = Triple;
      F = llvm::StringSwitch<DriverFlavor>(TripleRef)
              .StartsWith("hexagon", DriverFlavor::Hexagon)
              .StartsWith("arm", DriverFlavor::ARM_AArch64)
              .StartsWith("aarch64", DriverFlavor::ARM_AArch64)
              .StartsWith("riscv", DriverFlavor::RISCV32_RISCV64)
              .StartsWith("riscv32", DriverFlavor::RISCV32_RISCV64)
              .StartsWith("riscv64", DriverFlavor::RISCV32_RISCV64)
              .StartsWith("x86", DriverFlavor::x86_64);
    }
  }
  return std::make_pair(F, Triple);
}
