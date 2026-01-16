//===- GNUVerNeedFragment.cpp---------------------------------------------------===//
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
#include "eld/Fragment/GNUVerNeedFragment.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/Input/ELFDynObjectFile.h"
#include "eld/Input/InputFile.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/Object/ELF.h"
#include <cstdint>

using namespace eld;

GNUVerNeedFragment::GNUVerNeedFragment(ELFSection *S)
    : Fragment(Fragment::Type::GNUVerNeed, S) {}

template <class ELFT>
eld::Expected<void> GNUVerNeedFragment::computeVersionNeeds(
    const std::vector<InputFile *> &DynamicObjectFiles,
    ELFFileFormat *FileFormat, DiagnosticEngine &DE) {
  VerNeedEntrySize = sizeof(typename ELFT::Verneed);
  VernAuxEntrySize = sizeof(typename ELFT::Vernaux);
  bool traceSV = DE.getPrinter()->traceSymbolVersioning();
  for (auto *IF : DynamicObjectFiles) {
    auto *DynObjFile = llvm::cast<ELFDynObjectFile>(IF);
    const auto &VernAuxIDMap = DynObjFile->getOutputVernAuxIDMap();
    VerNeedInfo VNI;
    VNI.SONameOffset =
        FileFormat->addStringToDynStrTab(DynObjFile->getSOName());
    for (std::size_t i = 0; i < VernAuxIDMap.size(); ++i) {
      if (VernAuxIDMap[i] == 0)
        continue;
      auto *verdef = reinterpret_cast<const typename ELFT::Verdef *>(
          DynObjFile->getVerDef(i));
      llvm::StringRef VerName = DynObjFile->getDynStringTable().data() +
                                static_cast<size_t>(verdef->getAux()->vda_name);
      VernAuxInfo AuxInfo = {
          static_cast<uint32_t>(
              FileFormat->addStringToDynStrTab(VerName.str())),
          VernAuxIDMap[i], verdef->vd_hash};
      if (traceSV)
        DE.raise(Diag::trace_adding_verneed_entry)
            << VernAuxIDMap[i] << VerName.str();
      VNI.Vernauxs.push_back(AuxInfo);
    }
    if (!VNI.Vernauxs.empty())
      VersionNeeds.push_back(VNI);
  }
  return {};
}

size_t GNUVerNeedFragment::size() const {
  size_t VerNeedSize = VersionNeeds.size() * VerNeedEntrySize;
  size_t VernAuxSize = 0;
  for (const auto &VNI : VersionNeeds) {
    VernAuxSize += VNI.Vernauxs.size() * VernAuxEntrySize;
  }
  return VerNeedSize + VernAuxSize;
}

eld::Expected<void> GNUVerNeedFragment::emit(MemoryRegion &Mr, Module &M) {
  uint8_t *Buf = Mr.begin() + getOffset(M.getConfig().getDiagEngine());
  bool Is32Bits = M.getConfig().targets().is32Bits();
  if (Is32Bits) {
    return emitImpl<llvm::object::ELF32LE>(Buf, M);
  } else {
    return emitImpl<llvm::object::ELF64LE>(Buf, M);
  }
  return {};
}

template <class ELFT>
eld::Expected<void> GNUVerNeedFragment::emitImpl(uint8_t *Buf, Module &M) {
  auto *VerNeedBuf = reinterpret_cast<typename ELFT::Verneed *>(Buf);
  auto *VernAuxBuf = reinterpret_cast<typename ELFT::Vernaux *>(
      VerNeedBuf + VersionNeeds.size());
  for (const auto &VN : VersionNeeds) {
    VerNeedBuf->vn_version = 1;
    VerNeedBuf->vn_cnt = VN.Vernauxs.size();
    VerNeedBuf->vn_file = VN.SONameOffset;
    VerNeedBuf->vn_aux = reinterpret_cast<const char *>(VernAuxBuf) -
                         reinterpret_cast<const char *>(VerNeedBuf);
    VerNeedBuf->vn_next = sizeof(typename ELFT::Verneed);
    ++VerNeedBuf;
    for (const auto &VA : VN.Vernauxs) {
      llvm::errs() << "[GNUVerNeedFragment::emitImpl] Writing " << VA.VersionID
                   << " -> " << VA.VersionNameOffset << "\n";
      VernAuxBuf->vna_hash = VA.VersionNameHash;
      VernAuxBuf->vna_flags = 0;
      VernAuxBuf->vna_other = VA.VersionID;
      VernAuxBuf->vna_name = VA.VersionNameOffset;
      VernAuxBuf->vna_next = sizeof(typename ELFT::Vernaux);
      ++VernAuxBuf;
    }
    VernAuxBuf[-1].vna_next = 0;
  }
  VerNeedBuf[-1].vn_next = 0;
  return {};
}

template eld::Expected<void>
GNUVerNeedFragment::computeVersionNeeds<llvm::object::ELF32LE>(
    const std::vector<InputFile *> &DynamicObjectFiles,
    ELFFileFormat *FileFormat, DiagnosticEngine &DE);
template eld::Expected<void>
GNUVerNeedFragment::computeVersionNeeds<llvm::object::ELF64LE>(
    const std::vector<InputFile *> &DynamicObjectFiles,
    ELFFileFormat *FileFormat, DiagnosticEngine &DE);