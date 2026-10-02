//===- GNUVerDefFragment.cpp----------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===---------------------------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------------===//
#include "eld/Fragment/GNUVerDefFragment.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Fragment/DynStrFragment.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Path.h"
#include <cstdint>
#include <utility>

using namespace eld;

GNUVerDefFragment::GNUVerDefFragment(ELFSection *S, uint32_t Align)
    : Fragment(Fragment::Type::GNUVerDef, S, Align) {}

template <class ELFT>
eld::Expected<void>
GNUVerDefFragment::computeVersionDefs(Module &M, DynStrFragment *DynStr,
                                      DiagnosticEngine &DE) {
  const auto &VSNodes = M.getVersionScriptNodes();
  if (VSNodes.empty())
    return {};

  VerDefEntrySize = sizeof(typename ELFT::Verdef);
  VerdAuxEntrySize = sizeof(typename ELFT::Verdaux);

  const GeneralOptions &options = M.getConfig().options();
  std::string baseVersion = "";
  std::string SOName = options.soname();
  if (!SOName.empty())
    baseVersion = SOName;
  else {
    std::string outputFileName = options.outputFileName();
    baseVersion = std::string(llvm::sys::path::filename(outputFileName));
  }
  std::size_t baseVersionOffset = DynStr->addString(baseVersion);
  VersionDefs.push_back(VerDefInfo{1,
                                   static_cast<uint32_t>(baseVersionOffset),
                                   llvm::object::hashSysV(baseVersion),
                                   {}});

  uint16_t VerID = 2; // 0 and 1 are reserved
  for (const auto *Node : VSNodes) {
    // Only named nodes produce version definitions
    if (!Node || Node->isAnonymous())
      continue;
    llvm::StringRef VerName = Node->getName();
    std::size_t NameOffset = DynStr->addString(VerName.str());
    std::vector<uint32_t> DependencyNameOffsets;
    if (Node->hasDependency()) {
      std::size_t DependencyOffset =
          DynStr->addString(Node->getDependency().str());
      DependencyNameOffsets.push_back(static_cast<uint32_t>(DependencyOffset));
    }
    VerDefInfo Def{VerID, static_cast<uint32_t>(NameOffset),
                   llvm::object::hashSysV(VerName),
                   std::move(DependencyNameOffsets)};
    VersionDefs.push_back(std::move(Def));
    ++VerID;
  }
  return {};
}

size_t GNUVerDefFragment::size() const {
  size_t VerDefSize = VersionDefs.size() * VerDefEntrySize;
  size_t VerdAuxSize = 0;
  for (const auto &VD : VersionDefs)
    VerdAuxSize += (1 + VD.DependencyNameOffsets.size()) * VerdAuxEntrySize;
  return VerDefSize + VerdAuxSize;
}

eld::Expected<void> GNUVerDefFragment::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  bool Is32Bits = M.getConfig().targets().is32Bits();
  if (Is32Bits)
    return emitImpl<llvm::object::ELF32LE>(Buf, M);
  else
    return emitImpl<llvm::object::ELF64LE>(Buf, M);
}

template <class ELFT>
eld::Expected<void> GNUVerDefFragment::emitImpl(uint8_t *Buf, Module &M) {
  auto *VerDefBuf = reinterpret_cast<typename ELFT::Verdef *>(Buf);
  auto *VerdAuxBuf = reinterpret_cast<typename ELFT::Verdaux *>(
      VerDefBuf + VersionDefs.size());
  for (size_t I = 0; I < VersionDefs.size(); ++I) {
    const auto &VD = VersionDefs[I];
    const size_t AuxCount = 1 + VD.DependencyNameOffsets.size();
    uint16_t flags = (VD.VersionID == 1 ? llvm::ELF::VER_FLG_BASE : 0);
    VerDefBuf->vd_version = 1;
    VerDefBuf->vd_flags = flags;
    VerDefBuf->vd_ndx = VD.VersionID;
    VerDefBuf->vd_cnt = AuxCount;
    VerDefBuf->vd_hash = VD.VersionNameHash;
    VerDefBuf->vd_aux = reinterpret_cast<const char *>(VerdAuxBuf) -
                        reinterpret_cast<const char *>(VerDefBuf);
    VerDefBuf->vd_next =
        (I + 1 == VersionDefs.size()) ? 0 : sizeof(typename ELFT::Verdef);
    ++VerDefBuf;

    VerdAuxBuf->vda_name = VD.VersionNameOffset;
    VerdAuxBuf->vda_next = AuxCount == 1 ? 0 : sizeof(typename ELFT::Verdaux);
    ++VerdAuxBuf;

    for (size_t J = 0; J < VD.DependencyNameOffsets.size(); ++J) {
      VerdAuxBuf->vda_name = VD.DependencyNameOffsets[J];
      VerdAuxBuf->vda_next = (J + 1 == VD.DependencyNameOffsets.size())
                                 ? 0
                                 : sizeof(typename ELFT::Verdaux);
      ++VerdAuxBuf;
    }
  }
  return {};
}

// Explicit instantiations
template eld::Expected<void>
GNUVerDefFragment::computeVersionDefs<llvm::object::ELF32LE>(
    Module &M, DynStrFragment *DynStr, DiagnosticEngine &DE);
template eld::Expected<void>
GNUVerDefFragment::computeVersionDefs<llvm::object::ELF64LE>(
    Module &M, DynStrFragment *DynStr, DiagnosticEngine &DE);
template eld::Expected<void>
GNUVerDefFragment::emitImpl<llvm::object::ELF32LE>(uint8_t *Buf, Module &M);
template eld::Expected<void>
GNUVerDefFragment::emitImpl<llvm::object::ELF64LE>(uint8_t *Buf, Module &M);
