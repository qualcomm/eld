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
#include "eld/Driver/GnuLdDriver.h"
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

  // Parse all the options.
  Driver driver;

  Args = maybeExpandResponseFiles(Args, Alloc);
  if (!driver.setDriverFlavorAndInferredArchFromLinkCommand(Args))
    return 1;

  GnuLdDriver *LinkerDriver = driver.getLinkerDriver();

  int Status = LinkerDriver->link(Args);

  return Status;
}
