//===- Driver.cpp----------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//


#include "eld/Driver/Driver.h"
#include "eld/Driver/GnuLdDriver.h"
#include "eld/Support/Memory.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/TargetSelect.h"
#include "eld/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include <optional>

Driver::Driver(Flavor F, std::string Triple) : m_Flavor(F), m_Triple(Triple) {}

Driver *Driver::getLinker() {
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
    LinkDriver = GnuLdDriver::Create(m_Flavor, m_Triple);
    break;
  }
  default:
    break;
  }
  if (!LinkDriver)
    LinkDriver = GnuLdDriver::Create(
        getFlavorFromTarget(m_SupportedTargets.front()), m_Triple);
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

int Driver::link(llvm::ArrayRef<const char *> Args) {
  // If argv[0] is empty then use ld.eld.
  m_LinkerProgramName =
      (Args[0][0] ? llvm::sys::path::filename(Args[0]) : "ld.eld");
  return link(Args, getELDFlagsArgs());
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
