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

ELFObjectFile::ELFObjectFile(Input *I, DiagnosticEngine *diagEngine)
    : ELFFileBase(I, diagEngine, InputFile::ELFObjFileKind) {
  if (I->getSize())
    Contents = I->getFileContents();
}

void ELFObjectFile::setDynamicSections(ELFSection &GOT, ELFSection &GOTPLT,
                                       ELFSection &PLT, ELFSection &RelDyn,
                                       ELFSection &RelPLT) {
  m_GOT = &GOT;
  m_GOTPLT = &GOTPLT;
  m_PLT = &PLT;
  m_RelaDyn = &RelDyn;
  // m_RelaDyn link field is not set because .rel(a).dyn may include dynamic
  // relocations from sections other than .got. Therefore, its sh_info field is
  // zero.
  m_RelaPLT = &RelPLT;
  // .rel(a).plt has sh_info pointing to the .got.plt section, although, it's
  // not clear why this is needed for dynamic relocation sections.
  m_RelaPLT->setLink(m_GOTPLT);

  m_GOT->setExcludedFromGC();
  m_GOTPLT->setExcludedFromGC();
  m_PLT->setExcludedFromGC();
  m_RelaDyn->setExcludedFromGC();
  m_RelaPLT->setExcludedFromGC();
}

void ELFObjectFile::setPatchSections(ELFSection &GOTPatch,
                                     ELFSection &RelaPatch) {
  m_GOTPatch = &GOTPatch;
  m_RelaPatch = &RelaPatch;
  if (m_RelaPatch)
    m_RelaPatch->setLink(m_GOTPatch);
}

void ELFObjectFile::createDWARFContext(bool is32) {
  if (m_DebugSections.empty())
    return;

  llvm::StringMap<std::unique_ptr<llvm::MemoryBuffer>> DebugSections;
  uint32_t debugSectionNo = 0;
  for (Section *S : getSections()) {
    llvm::StringRef SectionName = S->name();
    SectionName = SectionName.substr(SectionName.find_first_not_of("._z"));
    if (!SectionName.starts_with("debug"))
      continue;
    DebugSections[SectionName] = std::move(m_DebugSections[debugSectionNo++]);
  }

  ASSERT(debugSectionNo == m_DebugSections.size(),
         "Debug Sections size mismatch");

  llvm::Expected<std::unique_ptr<llvm::DWARFContext>> DC =
      llvm::DWARFContext::create(DebugSections, is32 ? 4 : 8);

  // FIXME: Check for error from llvm::Expected here.
  if (DC)
    m_DWARFContext = std::move(*DC);
}

void ELFObjectFile::populateDebugSections() {
  for (const Section *S : getSections()) {
    llvm::StringRef SectionName = S->name();
    SectionName = SectionName.substr(SectionName.find_first_not_of("._z"));
    if (!SectionName.starts_with("debug"))
      continue;
    m_DebugSections.push_back(llvm::MemoryBuffer::getMemBuffer(
        llvm::dyn_cast<ELFSection>(S)->getContents(), /*BufferName*/ "",
        /*RequiresNullTerminator*/ false));
  }
}
