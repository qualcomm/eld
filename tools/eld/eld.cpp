//===- eld.cpp-------------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "eld/Config/Config.h"
#include "eld/Driver/Driver.h"
#include "eld/Support/Memory.h"
#include "eld/Support/TargetRegistry.h"
#include "eld/Support/TargetSelect.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::sys;
typedef std::vector<std::string> strings;



static std::string ParseProgName(llvm::StringRef ProgName) {
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

static std::pair<Flavor, std::string> getFlavor(StringRef S) {
  std::string Triple;
  Flavor F;
  F = StringSwitch<Flavor>(S)
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
    Triple = ParseProgName(S);
    if (!Triple.empty()) {
      llvm::StringRef TripleRef = Triple;
      F = StringSwitch<Flavor>(TripleRef)
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

// Check if the linker is Hexagon, ARM
static std::pair<Flavor, std::string> parseFlavor(const char *argv0) {
  // Deduct the flavor from argv[0].
  StringRef Arg0 = path::filename(argv0);
  if (Arg0.ends_with_insensitive(".exe"))
    Arg0 = Arg0.drop_back(4);
  return getFlavor(Arg0);
}

// If a command line option starts with "@", the driver reads its suffix as a
// file, parse its contents as a list of command line options, and insert them
// at the original @file position. If file cannot be read, @file is not expanded
// and left unmodified. @file can appear in a response file, so it's a recursive
// process.
static llvm::ArrayRef<const char *>
maybeExpandResponseFiles(llvm::ArrayRef<const char *> args,
                         BumpPtrAllocator &alloc) {
  // Expand response files.
  SmallVector<const char *, 256> smallvec;
  for (const char *arg : args)
    smallvec.push_back(arg);
  llvm::StringSaver saver(alloc);
  llvm::cl::ExpandResponseFiles(saver, llvm::cl::TokenizeGNUCommandLine,
                                smallvec);

  // Pack the results to a C-array and return it.
  const char **copy = alloc.Allocate<const char *>(smallvec.size() + 1);
  std::copy(smallvec.begin(), smallvec.end(), copy);
  copy[smallvec.size()] = nullptr;
  return llvm::ArrayRef(copy, smallvec.size() + 1);
}

/// Universal linker main().
int main(int Argc, const char **Argv) {
  // Standard set up, so program fails gracefully.
  llvm::BumpPtrAllocator Alloc;
  sys::PrintStackTraceOnErrorSignal(Argv[0]);
  PrettyStackTraceProgram StackPrinter(Argc, Argv);
  llvm_shutdown_obj Shutdown;

  std::vector<const char *> Args(Argv, Argv + Argc);

  Driver *driver = nullptr;

  std::pair<Flavor, std::string> P = parseFlavor(Argv[0]);

  // Parse all the options.
  driver = new Driver(P.first, P.second);

  if (nullptr == driver) {
    llvm::errs() << Argv[0] << "is not a recognized flavor"
                 << "\n";
    return LINK_FAIL;
  }

  Driver *Linker = driver->getLinker();

  bool Status =
      Linker->link(maybeExpandResponseFiles(Args, Alloc));

  delete driver;

  return Status;
}
