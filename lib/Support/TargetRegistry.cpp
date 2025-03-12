//===- TargetRegistry.cpp--------------------------------------------------===//
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

//===- TargetRegistry.cpp -------------------------------------------------===//
//===----------------------------------------------------------------------===//
#include "eld/Support/TargetRegistry.h"

using namespace eld;

TargetRegistry::TargetListTy eld::TargetRegistry::s_TargetList;

//===----------------------------------------------------------------------===//
// TargetRegistry
//===----------------------------------------------------------------------===//
void TargetRegistry::RegisterTarget(Target &Target, const char *Name,
                                    Target::TripleMatchQualityFnTy QualityFn) {
  Target.Name = Name;
  Target.TripleMatchQualityFn = QualityFn;
  Target.IsImplemented = true;

  s_TargetList.push_back(&Target);
}

const Target *TargetRegistry::lookupTarget(const std::string &CommandLineTriple,
                                           std::string &Error) {
  if (empty()) {
    Error = "Unable to find target for this triple (no target are registered)";
    return nullptr;
  }

  llvm::Triple Triple(CommandLineTriple);
  Target *Best = nullptr, *Ambiguity = nullptr;
  unsigned int Highest = 0;

  for (auto Target = begin(), Ie = end(); Target != Ie; ++Target) {
    unsigned int Quality = (*Target)->getTripleQuality(Triple);
    if (Quality > 0) {
      if (nullptr == Best || Highest < Quality) {
        Highest = Quality;
        Best = *Target;
        Ambiguity = nullptr;
      } else if (Highest == Quality) {
        Ambiguity = *Target;
      }
    }
  }

  if (nullptr == Best) {
    Error = "No available targets are compatible with this triple.";
  }
  if (nullptr != Ambiguity) {
    Error = std::string("Ambiguous targets: \"") + Best->name() + "\" and \"" +
            Ambiguity->name() + "\"";
    return nullptr;
  }

  return Best;
}

const Target *TargetRegistry::lookupTarget(const std::string &ArchName,
                                           llvm::Triple &Triple,
                                           std::string &Error) {
  const Target *Result = nullptr;
  if (!ArchName.empty()) {
    for (auto It = eld::TargetRegistry::begin(),
              Ie = eld::TargetRegistry::end();
         It != Ie; ++It) {
      if (ArchName == (*It)->name()) {
        Result = *It;
        break;
      }
    }

    if (nullptr == Result) {
      Error = std::string("invalid target '") + ArchName + "'.\n";
      return nullptr;
    }

    // Adjust the triple to match (if known), otherwise stick with the
    // module/host triple.
    llvm::Triple::ArchType Type =
        llvm::Triple::getArchTypeForLLVMName(ArchName);
    if (llvm::Triple::UnknownArch != Type)
      Triple.setArch(Type);
  } else {
    Result = lookupTarget(Triple.getTriple(), Error);
    if (nullptr == Result) {
      Error = std::string("unable to get target for `") + Triple.getTriple() +
              "'\n" + "(Detail: " + Error + ")\n";
      return nullptr;
    }
  }
  return Result;
}
