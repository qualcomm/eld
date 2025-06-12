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

Driver::Driver(Flavor F, std::string Triple)
    : DiagEngine(new eld::DiagnosticEngine(shouldColorize())),
      Config(DiagEngine), m_Flavor(F), m_Triple(Triple) {
  std::unique_ptr<eld::DiagnosticInfos> DiagInfo =
      std::make_unique<eld::DiagnosticInfos>(Config);
  DiagEngine->setInfoMap(std::move(DiagInfo));
}

Driver::~Driver() { delete DiagEngine; }

GnuLdDriver *Driver::getLinker() {
  if (!m_SupportedTargets.size())
    InitTarget();
  GnuLdDriver *LinkDriver = nullptr;
  switch (m_Flavor) {
  case Flavor::Hexagon:
  case Flavor::ARM:
  case Flavor::AArch64:
  case Flavor::RISCV32:
  case Flavor::RISCV64:
  case Flavor::x86_64: {
    LinkDriver = GnuLdDriver::Create(Config, m_Flavor, m_Triple);
    break;
  }
  default:
    break;
  }
  if (!LinkDriver)
    LinkDriver = GnuLdDriver::Create(
        Config, getFlavorFromTarget(m_SupportedTargets.front()), m_Triple);
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
  eld::InitializeAllLinkers();
  eld::InitializeAllEmulations();

  for (auto it = eld::TargetRegistry::begin(); it != eld::TargetRegistry::end();
       ++it) {
    std::string TargetName = getStringFromTarget((*it)->Name);
    if (TargetName.empty())
      continue;
    auto STIter = std::find(m_SupportedTargets.begin(),
                            m_SupportedTargets.end(), TargetName);
    if (STIter == m_SupportedTargets.end())
      m_SupportedTargets.emplace_back(TargetName);
  }
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

Flavor Driver::getFlavorFromTarget(llvm::StringRef Target) const {
  return llvm::StringSwitch<Flavor>(Target)
      .CaseLower("hexagon", Flavor::Hexagon)
      .CaseLower("arm", Flavor::ARM)
      .CaseLower("aarch64", Flavor::AArch64)
      .CaseLower("riscv", Flavor::RISCV32)
      .CaseLower("x86_64", Flavor::x86_64)
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

bool Driver::setFlavorAndTripleFromLinkCommand(
    llvm::ArrayRef<const char *> Args) {
  auto ExpFlavorAndTriple = getFlavorAndTripleFromLinkCommand(Args);
  if (!ExpFlavorAndTriple) {
    DiagEngine->raiseDiagEntry(std::move(ExpFlavorAndTriple.error()));
    return false;
  }
  auto FlavorAndTriple = ExpFlavorAndTriple.value();
  m_Flavor = FlavorAndTriple.first;
  m_Triple = FlavorAndTriple.second;
  return true;
}

eld::Expected<std::pair<Flavor, std::string>>
Driver::getFlavorAndTripleFromLinkCommand(llvm::ArrayRef<const char *> Args) {
  auto FlavorAndTriple = Driver::parseFlavorAndTripleFromProgramName(Args[0]);
  if (FlavorAndTriple.first != Flavor::Invalid)
    return FlavorAndTriple;

  // We read the emulation options here to just select the driver.
  // Emulation options are properly handled by the driver.
  // Thus, the flavor selected here might not be accurate. But that's
  // alright as long as the right driver is selected. For example,
  // we set the Flavor to Flavor::RISCV32 for both riscv32 and riscv64
  // emulations. It is fine because RISCVLinDriver will see the emulation
  // options for riscv64 and properly set the emulation to riscv64.
  OPT_GnuLdOptTable Table;
  unsigned MissingIndex;
  unsigned MissingCount;
  llvm::opt::InputArgList ArgList =
      Table.ParseArgs(Args.slice(1), MissingIndex, MissingCount);
  Flavor F = Flavor::Invalid;
  if (llvm::opt::Arg *Arg = ArgList.getLastArg(OPT_GnuLdOptTable::emulation)) {
    std::string Emulation = Arg->getValue();
#if defined(ELD_ENABLE_TARGET_HEXAGON)
    if (HexagonLinkDriver::isValidEmulation(Emulation))
      F = Flavor::Hexagon;
#endif
#if defined(ELD_ENABLE_TARGET_RISCV)
    // It is okay to consider RISCV64 emulation as RISCV32 flavor
    // here because RISCVLinkDriver will properly set the emulation.
    if (RISCVLinkDriver::isSupportedEmulation(Emulation))
      F = Flavor::RISCV32;
#endif
#if defined(ELD_ENABLE_TARGET_ARM) || defined(ELD_ENABLE_TARGET_AARCH64)
    std::optional<llvm::Triple> optTriple =
        ARMLinkDriver::ParseEmulation(Emulation, DiagEngine);
    if (optTriple.has_value()) {
      llvm::Triple EmulationTriple = optTriple.value();
      if (EmulationTriple.getArch() == llvm::Triple::arm)
        F = Flavor::ARM;
      else if (EmulationTriple.getArch() == llvm::Triple::aarch64)
        F = Flavor::AArch64;
    }
#endif
#if defined(ELD_ENABLE_TARGET_X86_64)
    if (x86_64LinkDriver::isValidEmulation(Emulation))
      F = Flavor::x86_64;
#endif
    if (F == Flavor::Invalid)
      return std::make_unique<eld::DiagnosticEntry>(
          eld::Diag::fatal_unsupported_emulation,
          std::vector<std::string>{Emulation});
  }
  return std::pair<Flavor, std::string>{F, ""};
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

std::pair<Flavor, std::string>
Driver::parseFlavorAndTripleFromProgramName(const char *argv0) {
  // Deduct the flavor from argv[0].
  llvm::StringRef ProgramName = llvm::sys::path::filename(argv0);
  if (ProgramName.ends_with_insensitive(".exe"))
    ProgramName = ProgramName.drop_back(4);
  std::string Triple;
  Flavor F;
  F = llvm::StringSwitch<Flavor>(ProgramName)
          .Case("hexagon-link", Flavor::Hexagon)
          .Case("hexagon-linux-link", Flavor::Hexagon)
          .Case("arm-link", Flavor::ARM)
          .Case("aarch64-link", Flavor::AArch64)
          .Case("x86_64-link", Flavor::x86_64)
          .Case("riscv-link", Flavor::RISCV32)
          .Case("riscv32-link", Flavor::RISCV32)
          .Case("riscv64-link", Flavor::RISCV64)
          .Default(Invalid);
  // Try to get the Flavor from the triple.
  if (F == Invalid) {
    Triple = parseProgName(ProgramName);
    if (!Triple.empty()) {
      llvm::StringRef TripleRef = Triple;
      F = llvm::StringSwitch<Flavor>(TripleRef)
              .StartsWith("hexagon", Flavor::Hexagon)
              .StartsWith("arm", Flavor::ARM)
              .StartsWith("aarch64", Flavor::AArch64)
              .StartsWith("riscv", Flavor::RISCV32)
              .StartsWith("riscv32", Flavor::RISCV32)
              .StartsWith("riscv64", Flavor::RISCV64)
              .StartsWith("x86", Flavor::x86_64);
    }
  }
  return std::make_pair(F, Triple);
}
