//===- ExecELFReader.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Readers/ExecELFReader.h"
#include "DiagnosticEntry.h"
#include "eld/Core/Module.h"
#include "eld/Input/ELFObjectFile.h"
#include "eld/PluginAPI/Expected.h"
#include "eld/Readers/ELFReaderBase.h"

using namespace eld;

template <class ELFT>
eld::Expected<bool> ExecELFReader<ELFT>::readSectionHeaders() {
  ASSERT(this->m_LLVMELFFile, "m_LLVMELFFile must be initialized!");
  if (!this->m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(this->m_RawSectHdrs,
                              this->m_LLVMELFFile->sections());
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

  if (this->m_RawSectHdrs->empty())
    return true;

  /// Create all sections, including the first null section.
  for (const typename ELFReader<ELFT>::Elf_Shdr &rawSectHdr :
       this->m_RawSectHdrs.value()) {
    eld::Expected<ELFSection *> expSection = this->createSection(rawSectHdr);
    ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSection);
    ELFSection *S = expSection.value();
    this->setSectionInInputFile(S, rawSectHdr);
    this->setSectionAttributes(S, rawSectHdr);
  }

  this->setLinkInfoAttributes();
  // FIXME: This doesn't return the result of verifyFile to maintain
  // compatibility with the old readers.
  this->verifyFile(this->m_Module.getConfig().getDiagEngine());

  auto &PM = this->m_Module.getPluginManager();
  if (!PM.callVisitSectionsHook(this->m_InputFile))
    return false;

  return true;
}

template <class ELFT>
eld::Expected<ELFSection *> ExecELFReader<ELFT>::createSection(
    typename ELFReader<ELFT>::Elf_Shdr rawSectHdr) {
  Module &module = this->m_Module;
  eld::Expected<std::string> expSectName = this->getSectionName(rawSectHdr);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expSectName);
  std::string sectName = expSectName.value();

  // Setup all section properties.
  // FIXME: sectName can be extracted from rawSectHdr.
  LDFileFormat::Kind kind = this->getSectionKind(rawSectHdr, sectName);

  // FIXME: Emit some diagnostic here.
  if (kind == LDFileFormat::Error)
    return static_cast<ELFSection *>(nullptr);

  bool SectionIsIgnore = false;
  // Embedded bitcode sections must be not regarded in linking. However they
  // must be added to context since the sh_info field of relocation sections
  // that follow these sections will need to have proper index in section
  // header table. We thus read them and then set to ignore to have no effect
  // on linking.
  if (sectName == ".llvmbc" || sectName == ".llvmcmd")
    SectionIsIgnore = true;

  if (this->getInputFile()->getInput()->getAttribute().isPatchBase()) {
    // When reading the executable base image in the patch link, we ignore all
    // the sections except those explicitly mentioned below.
    if (!((sectName == ".pgot" || sectName == ".rel.pgot" ||
           sectName == ".rela.pgot") &&
          (kind == LDFileFormat::Regular || kind == LDFileFormat::Relocation)))
      SectionIsIgnore = true;
  } else {
    if (kind == LDFileFormat::EhFrame)
      return module.getScript().sectionMap().createEhFrameSection(
          sectName, rawSectHdr.sh_type, rawSectHdr.sh_flags,
          rawSectHdr.sh_entsize);
  }

  return module.getScript().sectionMap().createELFSection(
      sectName, (SectionIsIgnore ? LDFileFormat::Discard : kind),
      rawSectHdr.sh_type, rawSectHdr.sh_flags, rawSectHdr.sh_entsize);
}

template <class ELFT>
eld::Expected<bool> ExecELFReader<ELFT>::readRelocationSection(ELFSection *RS) {
  ASSERT(RS->getType() == llvm::ELF::SHT_REL ||
             RS->getType() == llvm::ELF::SHT_RELA,
         "RS must be a relocation section!");
  if (RS->isIgnore() || RS->isDiscard())
    return true;
  eld::Expected<bool> expReadRelocSect =
      (RS->getType() == llvm::ELF::SHT_RELA
           ? readRelocationSection</*isRela=*/true>(RS)
           : readRelocationSection</*isRela=*/false>(RS));
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expReadRelocSect);
  return expReadRelocSect.value();
}

template <class ELFT>
template <bool isRela>
eld::Expected<bool> ExecELFReader<ELFT>::readRelocationSection(ELFSection *RS) {
  ASSERT(this->m_LLVMELFFile.has_value(), "m_LLVMELFFile must be initialized!");
  if (!this->m_RawSectHdrs)
    LLVMEXP_EXTRACT_AND_CHECK(this->m_RawSectHdrs,
                              this->m_LLVMELFFile->sections());
  ASSERT(this->m_RawSectHdrs, "m_RawSectHdrs must be initialized!");

  const typename ELFReader<ELFT>::Elf_Shdr &rawS =
      (*this->m_RawSectHdrs)[RS->getIndex()];

  auto expRelRange = ELFReader<ELFT>::template getRelocations<isRela>(rawS);
  ELDEXP_RETURN_DIAGENTRY_IF_ERROR(expRelRange);
  auto relRange = std::move(expRelRange.value());

  GNULDBackend &backend = *(this->m_Module.getBackend());
  InputFile *inputFile = this->getInputFile();
  ELFFileBase *EFile = llvm::cast<ELFFileBase>(inputFile);

  for (const auto &R : relRange) {
    uint32_t rSym = R.getSymbol(/*isMips=*/false);
    LDSymbol *symbol = EFile->getSymbol(rSym);
    if (!symbol) {
      return std::make_unique<plugin::DiagnosticEntry>(plugin::DiagnosticEntry(
          diag::err_cannot_read_symbol,
          {std::to_string(rSym),
           inputFile->getInput()->getResolvedPath().getFullPath()}));
    }

    ELFSection *linkSect = RS->getLink();
    Relocation::Type rType = this->template getRelocationType<isRela>(R);
    typename ELFReader<ELFT>::intX_t rAddend = this->getAddend(R);

    uint64_t offset = R.r_offset - linkSect->addr();
    if (backend.handleRelocation(linkSect, rType, *symbol, offset, rAddend))
      continue;

    Relocation *relocation = eld::IRBuilder::AddRelocation(
        backend.getRelocator(), linkSect, rType, *symbol, offset, rAddend);
    if (relocation)
      linkSect->addRelocation(relocation);
  }
  return backend.handlePendingRelocations(RS->getLink());
}

template <class ELFT>
eld::Expected<std::unique_ptr<ExecELFReader<ELFT>>>
ExecELFReader<ELFT>::Create(Module &module, InputFile &inputFile) {
  ASSERT(inputFile.isExecutableELFFile(),
         "ExecELFReader must only be used for executable object files.");
  plugin::DiagnosticEntry diagEntry;
  ExecELFReader<ELFT> reader =
      ExecELFReader<ELFT>(module, inputFile, diagEntry);
  if (diagEntry)
    return std::make_unique<plugin::DiagnosticEntry>(diagEntry);
  return std::make_unique<ExecELFReader<ELFT>>(reader);
}

template <class ELFT>
ExecELFReader<ELFT>::ExecELFReader(Module &module, InputFile &inputFile,
                                   plugin::DiagnosticEntry &diagEntry)
    : ELFReader<ELFT>(module, inputFile, diagEntry) {}

namespace eld {
template class ExecELFReader<llvm::object::ELF32LE>;
template class ExecELFReader<llvm::object::ELF64LE>;
} // namespace eld
