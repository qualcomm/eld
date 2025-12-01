#include "eld/Fragment/GNUVerDefFragment.h"
#include "eld/Config/GeneralOptions.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/SymbolResolver/ResolveInfo.h"
#include "eld/Target/ELFFileFormat.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELF.h"
#include <cstdint>

using namespace eld;

GNUVerDefFragment::GNUVerDefFragment(ELFSection *S)
    : Fragment(Fragment::Type::GNUVerDef, S) {}

template <class ELFT>
eld::Expected<void>
GNUVerDefFragment::computeVersionDefs(Module &M, ELFFileFormat *FileFormat,
                                      DiagnosticEngine &DE) {
  VerDefEntrySize = sizeof(typename ELFT::Verdef);
  VerdAuxEntrySize = sizeof(typename ELFT::Verdaux);

  const GeneralOptions &options = M.getConfig().options();
  std::string baseVersion = "";
  std::string SOName = options.soname();
  if (!SOName.empty())
    baseVersion = SOName;
  else
    baseVersion = options.outputFileName();
  std::size_t baseVersionOffset = FileFormat->addStringToDynStrTab(baseVersion);
  VersionDefs.push_back(VerDefInfo{1, static_cast<uint32_t>(baseVersionOffset),
                                   llvm::object::hashSysV(baseVersion)});

  const auto &VSNodes = M.getVersionScriptNodes();
  uint16_t VerID = 2; // 0 and 1 are reserved
  for (const auto *Node : VSNodes) {
    // Only named nodes produce version definitions
    if (!Node || Node->isAnonymous())
      continue;
    llvm::StringRef VerName = Node->getName();
    std::size_t NameOffset = FileFormat->addStringToDynStrTab(VerName.str());
    VersionDefs.push_back(VerDefInfo{VerID, static_cast<uint32_t>(NameOffset),
                                     llvm::object::hashSysV(VerName)});
    ++VerID;
  }
  return {};
}

size_t GNUVerDefFragment::size() const {
  size_t VerDefSize = VersionDefs.size() * VerDefEntrySize;
  size_t VerdAuxSize = VersionDefs.size() * VerdAuxEntrySize; // one aux/name
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
  for (const auto &VD : VersionDefs) {
    uint16_t flags = (VD.VersionID == 1 ? llvm::ELF::VER_FLG_BASE : 0);
    VerDefBuf->vd_version = 1;
    VerDefBuf->vd_flags = flags;
    VerDefBuf->vd_ndx = VD.VersionID;
    VerDefBuf->vd_cnt = 1; // one aux: name
    VerDefBuf->vd_hash = VD.VersionNameHash;
    VerDefBuf->vd_aux = reinterpret_cast<const char *>(VerdAuxBuf) -
                        reinterpret_cast<const char *>(VerDefBuf);
    VerDefBuf->vd_next = sizeof(typename ELFT::Verdef);
    ++VerDefBuf;

    VerdAuxBuf->vda_name = VD.VersionNameOffset;
    VerdAuxBuf->vda_next = sizeof(typename ELFT::Verdaux);
    ++VerdAuxBuf;
  }
  // Terminate aux and def chains
  if (!VersionDefs.empty()) {
    VerdAuxBuf[-1].vda_next = 0;
    VerDefBuf[-1].vd_next = 0;
  }
  return {};
}

// Explicit instantiations
template eld::Expected<void>
GNUVerDefFragment::computeVersionDefs<llvm::object::ELF32LE>(
    Module &M, ELFFileFormat *FileFormat, DiagnosticEngine &DE);
template eld::Expected<void>
GNUVerDefFragment::computeVersionDefs<llvm::object::ELF64LE>(
    Module &M, ELFFileFormat *FileFormat, DiagnosticEngine &DE);
template eld::Expected<void>
GNUVerDefFragment::emitImpl<llvm::object::ELF32LE>(uint8_t *Buf, Module &M);
template eld::Expected<void>
GNUVerDefFragment::emitImpl<llvm::object::ELF64LE>(uint8_t *Buf, Module &M);
