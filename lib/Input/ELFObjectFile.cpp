//===- ELFObjectFile.cpp---------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "eld/Input/ELFObjectFile.h"
#include "eld/Input/Input.h"
#include "eld/Input/InputFile.h"
#include "eld/Readers/ELFSection.h"
#include "eld/Support/Memory.h"
#include "eld/Support/MsgHandling.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace eld;

ELFObjectFile::ELFObjectFile(Input *I, DiagnosticEngine *DiagEngine)
    : ELFFileBase(I, DiagEngine, InputFile::ELFObjFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}

void ELFObjectFile::setDynamicSections(ELFSection &PGOT, ELFSection &PGOTPLT,
                                       ELFSection &PPLT, ELFSection &PRelDyn,
                                       ELFSection &PRelPLT) {
  GOT = &PGOT;
  GOTPLT = &PGOTPLT;
  PLT = &PPLT;
  RelaDyn = &PRelDyn;
  // RelaDyn link field is not set because .rel(a).dyn may include dynamic
  // relocations from sections other than .got. Therefore, its sh_info field is
  // zero.
  RelaPLT = &PRelPLT;
  // .rel(a).plt has sh_info pointing to the .got.plt section, although, it's
  // not clear why this is needed for dynamic relocation sections.
  RelaPLT->setLink(GOTPLT);

  GOT->setExcludedFromGC();
  GOTPLT->setExcludedFromGC();
  PLT->setExcludedFromGC();
  RelaDyn->setExcludedFromGC();
  RelaPLT->setExcludedFromGC();
}

void ELFObjectFile::setPatchSections(ELFSection &PGOTPatch,
                                     ELFSection &PRelaPatch) {
  GOTPatch = &PGOTPatch;
  RelaPatch = &PRelaPatch;
  if (RelaPatch)
    RelaPatch->setLink(GOTPatch);
}

void ELFObjectFile::createDWARFContext(bool Is32) {
  if (DebugSections.empty())
    return;

  llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> DebugSectMap;
  uint32_t DebugSectionNo = 0;
  for (Section *S : getSections()) {
    llvm::StringRef SectionName = S->name();
    SectionName = SectionName.substr(SectionName.find_first_not_of("._z"));
    if (!SectionName.starts_with("debug"))
      continue;
    DebugSectMap[SectionName] = std::move(DebugSections[DebugSectionNo++]);
  }

  ASSERT(DebugSectionNo == DebugSections.size(),
         "Debug Sections size mismatch");

  llvm::Expected<std::unique_ptr<llvm::DWARFContext>> DC =
      llvm::DWARFContext::create(DebugSectMap, Is32 ? 4 : 8);

  // FIXME: Check for error from llvm::Expected here.
  if (DC)
    DWARFContext = std::move(*DC);
}

void ELFObjectFile::populateDebugSections() {
  for (const Section *S : getSections()) {
    llvm::StringRef SectionName = S->name();
    SectionName = SectionName.substr(SectionName.find_first_not_of("._z"));
    if (!SectionName.starts_with("debug"))
      continue;
    DebugSections.push_back(llvm::MemoryBuffer::getMemBuffer(
        llvm::dyn_cast<ELFSection>(S)->getContents(), /*BufferName*/ "",
        /*RequiresNullTerminator*/ false));
  }
}
