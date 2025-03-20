//===- ELFReaderBase.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ELFReaderBase.h"
#include "eld/Config/LinkerConfig.h"
#include "eld/Core/Module.h"
#include "eld/Diagnostics/DiagnosticEngine.h"
#include "eld/PluginAPI/DiagnosticEntry.h"
#include "eld/Readers/DynamicELFReader.h"
#include "eld/Readers/ELFReader.h"
#include "eld/Readers/ExecELFReader.h"
#include "eld/Readers/RelocELFReader.h"
#include "llvm/Support/ErrorHandling.h"
#include <type_traits>

using namespace eld;

ELFReaderBase::ELFReaderBase(Module &module, InputFile &inputFile)
    : m_Module(module), m_InputFile(inputFile) {}

eld::Expected<std::unique_ptr<ELFReaderBase>>
ELFReaderBase::Create(Module &module, InputFile &inputFile) {
  plugin::DiagnosticEntry diagEntry;
  std::unique_ptr<ELFReaderBase> reader;
  LinkerConfig &config = module.getConfig();
  if (config.targets().isBigEndian())
    return std::make_unique<plugin::DiagnosticEntry>(
        plugin::DiagnosticEntry(Diag::fatal_big_endian_target,
                                {inputFile.getInput()->decoratedPath()}));

  if (config.targets().is32Bits())
    return createImpl<llvm::object::ELF32LE>(module, inputFile);
  else if (config.targets().is64Bits())
    return createImpl<llvm::object::ELF64LE>(module, inputFile);
  else {
    if (config.targets().hasTriple()) {
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::DiagnosticEntry(Diag::fatal_unsupported_target,
                                  {config.targets().triple().getTriple()}));
    } else
      return std::make_unique<plugin::DiagnosticEntry>(
          plugin::DiagnosticEntry(Diag::fatal_missing_target_triple, {}));
  }
}

template <class ELFT>
eld::Expected<std::unique_ptr<ELFReaderBase>>
ELFReaderBase::createImpl(Module &module, InputFile &inputFile) {
  if (inputFile.isObjectFile()) {
    auto expReader = RelocELFReader<ELFT>::Create(module, inputFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
    return std::move(expReader.value());
  }

  if (inputFile.isDynamicLibrary()) {
    auto expReader = DynamicELFReader<ELFT>::Create(module, inputFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
    return std::move(expReader.value());
  }

  if (inputFile.isExecutableELFFile()) {
    auto expReader = ExecELFReader<ELFT>::Create(module, inputFile);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReader);
    return std::move(expReader.value());
  }

  llvm_unreachable("Unsupported ELF object type!");
}

void ELFReaderBase::setSymbolsAliasInfo() {
  ASSERT(0,
         "setSymbolsAliasInfo must only be called for dynamic object files.");
}

ResolveInfo::Type ELFReaderBase::getSymbolType(uint8_t info, uint32_t shndx) {
  ResolveInfo::Type result = static_cast<ResolveInfo::Type>(info & 0xF);
  if (shndx == llvm::ELF::SHN_ABS && result == ResolveInfo::Section) {
    return ResolveInfo::Object;
  }
  return result;
}

ResolveInfo::Desc ELFReaderBase::getSymbolDesc(const GNULDBackend &backend,
                                               uint32_t shndx, uint8_t binding,
                                               InputFile &inputFile,
                                               bool isOrdinary) {
  if (isOrdinary) {
    if (shndx == llvm::ELF::SHN_UNDEF)
      return ResolveInfo::Undefined;

    if (shndx == llvm::ELF::SHN_ABS)
      return ResolveInfo::Define;

    if (shndx == llvm::ELF::SHN_COMMON)
      return ResolveInfo::Common;

    if (shndx >= llvm::ELF::SHN_LOPROC && shndx <= llvm::ELF::SHN_HIPROC) {
      ResolveInfo::Desc desc = backend.getSymDesc(shndx);
      if (desc != ResolveInfo::NoneDesc)
        return desc;
    }
  }

  ELFFileBase *ObjFile = llvm::dyn_cast<ELFFileBase>(&inputFile);
  // an ELF symbol defined in a section which we are not including
  // must be treated as undefined.
  if (nullptr == ObjFile->getELFSection(shndx) ||
      (binding != llvm::ELF::STB_LOCAL &&
       (ObjFile->getELFSection(shndx)->isIgnore() ||
        ObjFile->getELFSection(shndx)->isDiscard()))) {
    // Sections of patch-base symbols are not loaded, but the symbols will be
    // converted to absolute later in IRBuilder::addSymbol.
    if (!inputFile.getInput()->getAttribute().isPatchBase())
      return ResolveInfo::Undefined;
  }

  return ResolveInfo::Define;
}

ResolveInfo::Binding ELFReaderBase::getSymbolBinding(uint8_t binding,
                                                     uint32_t shndx,
                                                     ELFFileBase &EFileBase) {
  switch (binding) {
  case llvm::ELF::STB_LOCAL:
    return ResolveInfo::Local;
  case llvm::ELF::STB_GLOBAL: {
    ELFSection *S = EFileBase.getELFSection(shndx);
    // If we have a section by that section index, the symbol is not really an
    // absolute symbol. Its still associated as a GLOBAL symbol by what the
    // object file contained binding was.
    if (S && !S->isIgnore() && !S->isDiscard())
      return ResolveInfo::Global;
    if (shndx == llvm::ELF::SHN_ABS)
      return ResolveInfo::Absolute;
    return ResolveInfo::Global;
  }
  case llvm::ELF::STB_WEAK:
    return ResolveInfo::Weak;
  }

  return ResolveInfo::NoneBinding;
}

eld::Expected<ResolveInfo *>
ELFReaderBase::computeGroupSignature(const ELFSection *S) const {
  ASSERT(0, "setPostLTOCommonSymbolsOrigin must only be called for relocatable "
            "object files.");
  return static_cast<ResolveInfo *>(nullptr);
}

eld::Expected<std::vector<uint32_t>>
ELFReaderBase::getGroupMemberIndices(const ELFSection *S) const {
  ASSERT(0, "getGroupMemberIndices must only be called for relocatable "
            "object files.");
  return {};
}

eld::Expected<uint32_t> ELFReaderBase::getGroupFlag(const ELFSection *S) const {
  ASSERT(0, "getGroupFlag must only be called for relocatable "
            "object files.");
  return {};
}

eld::Expected<bool> ELFReaderBase::readCompressedSection(ELFSection *S) {
  ASSERT(0, "readCompressedSection must only be called for relocatable "
            "object files.");
  return false;
}

eld::Expected<bool> ELFReaderBase::readMergeStringSection(ELFSection *S) {
  ASSERT(0, "readMergeStringSection must only be called for relocatable "
            "object files.");
  return false;
}

void ELFReaderBase::setPostLTOCommonSymbolsOrigin() const {
  ASSERT(0, "setPostLTOCommonSymbolsOrigin must only be called for relocatable "
            "object files.");
}

eld::Expected<bool> ELFReaderBase::readOneGroup(ELFSection *S) {
  ASSERT(0, "readOneGroup must only be called for relocatable "
            "object files.");
  return false;
}

eld::Expected<bool> ELFReaderBase::readRelocationSection(ELFSection *RS) {
  ASSERT(0, "readRelocationSection must only be called for relocatable "
            "object files.");
}
